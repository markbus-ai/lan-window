#include "common/receiver_pipeline.h"

#include "common/gst_util.h"

#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

namespace lanwindow {

ReceiverPipeline::ReceiverPipeline(QObject *parent) : QObject(parent)
{
    m_pollTimer.setTimerType(Qt::PreciseTimer);
    m_pollTimer.setInterval(4);
    connect(&m_pollTimer, &QTimer::timeout, this, &ReceiverPipeline::poll);
}

ReceiverPipeline::~ReceiverPipeline()
{
    stop();
}

bool ReceiverPipeline::start(const ReceiverConfig &cfg, QString *error)
{
    stop();
    ensureGstInit();
    auto fail = [&](const QString &msg) {
        m_appsink = nullptr;
        destroyPipeline(&m_pipeline);
        if (error)
            *error = msg;
        return false;
    };

    const QString invalid = validateReceiverConfig(cfg);
    if (!invalid.isEmpty())
        return fail(invalid);
    const QStringList missing = missingElements(requiredReceiverElements());
    if (!missing.isEmpty())
        return fail(QStringLiteral("Faltan elementos GStreamer: %1").arg(missing.join(", ")));

    QString err;
    m_pipeline = gst_pipeline_new("receiver");
    std::vector<GstElement *> chain;
    auto add = [&](const char *factory, const char *name) -> GstElement * {
        GstElement *e = makeElement(factory, name, &err);
        if (e) {
            gst_bin_add(GST_BIN(m_pipeline), e);
            chain.push_back(e);
        }
        return e;
    };
    GstElement *src = add("udpsrc", "src");
    GstElement *jitter = add("rtpjitterbuffer", "jitter");
    GstElement *depay = add("rtph264depay", "depay");
    add("h264parse", "parse");
    GstElement *dec = add("avdec_h264", "decoder");
    add("videoconvert", "convert");
    m_appsink = add("appsink", "sink");
    if (!err.isEmpty())
        return fail(err);

    const QByteArray addr = cfg.bindAddress.toUtf8(); // validado como IPv4 estricta
    GstCaps *rtpCaps = gst_caps_from_string("application/x-rtp,media=video,clock-rate=90000,"
                                            "encoding-name=H264,payload=96");
    g_object_set(src, "address", addr.constData(), "port", cfg.port, "caps", rtpCaps, "buffer-size",
                 4 * 1024 * 1024, "reuse", FALSE, nullptr);
    gst_caps_unref(rtpCaps);
    g_object_set(jitter, "latency", guint(cfg.latencyMs), "drop-on-latency", TRUE, nullptr);
    g_object_set(depay, "wait-for-keyframe", TRUE, nullptr);
    // Hilos por slice: el decodificado por fotogramas añadiría varios fotogramas de retraso.
    gst_util_set_object_arg(G_OBJECT(dec), "thread-type", "slice");
    g_object_set(dec, "output-corrupt", FALSE, nullptr);
    GstCaps *rgba = gst_caps_from_string("video/x-raw,format=RGBA");
    g_object_set(m_appsink, "caps", rgba, "sync", FALSE, "max-buffers", 1u, "drop", TRUE, "emit-signals",
                 FALSE, nullptr);
    gst_caps_unref(rgba);

    for (size_t i = 0; i + 1 < chain.size(); ++i) {
        if (!gst_element_link(chain[i], chain[i + 1]))
            return fail(QStringLiteral("No se pudieron enlazar %1 y %2.")
                            .arg(QString::fromUtf8(GST_OBJECT_NAME(chain[i])),
                                 QString::fromUtf8(GST_OBJECT_NAME(chain[i + 1]))));
    }

    // READY abre el socket: así detectamos puerto ocupado/dirección inválida y leemos el puerto real.
    if (gst_element_set_state(m_pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE
        || gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        QString msg = QStringLiteral("No se pudo abrir %1:%2 (¿puerto en uso o dirección no local?).")
                          .arg(cfg.bindAddress)
                          .arg(cfg.port);
        for (const BusEvent &ev : drainBus(m_pipeline))
            if (ev.type == BusEvent::Error)
                msg += QLatin1Char(' ') + ev.text;
        return fail(msg);
    }
    gint port = 0;
    g_object_get(src, "port", &port, nullptr);
    m_boundPort = port;
    m_signalTimeoutMs = cfg.signalTimeoutMs;
    m_frameCount = 0;
    m_hasSignal = false;
    m_lastFrame = QImage();
    m_sinceLastFrame.start();
    m_pollTimer.start();
    return true;
}

void ReceiverPipeline::stop()
{
    m_pollTimer.stop();
    m_appsink = nullptr;
    destroyPipeline(&m_pipeline);
    m_boundPort = 0;
    m_lastFrame = QImage();
    if (m_hasSignal) {
        m_hasSignal = false;
        emit signalChanged(false);
    }
}

void ReceiverPipeline::poll()
{
    for (const BusEvent &ev : drainBus(m_pipeline)) {
        if (ev.type == BusEvent::Warning) {
            emit warning(ev.text);
        } else {
            const QString text = ev.type == BusEvent::Eos ? QStringLiteral("Fin de flujo inesperado.") : ev.text;
            stop();
            emit failed(text);
            return;
        }
    }
    if (!m_appsink)
        return;

    // Sólo interesa el fotograma más reciente.
    GstSample *sample = nullptr;
    while (GstSample *s = gst_app_sink_try_pull_sample(GST_APP_SINK(m_appsink), 0)) {
        if (sample)
            gst_sample_unref(sample);
        sample = s;
    }

    if (sample) {
        GstVideoInfo info;
        GstCaps *caps = gst_sample_get_caps(sample); // se relee en cada muestra: la resolución puede cambiar
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstVideoFrame frame;
        if (caps && buffer && gst_video_info_from_caps(&info, caps)
            && GST_VIDEO_INFO_FORMAT(&info) == GST_VIDEO_FORMAT_RGBA
            && gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ)) {
            const QImage view(static_cast<const uchar *>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0)),
                              GST_VIDEO_FRAME_WIDTH(&frame), GST_VIDEO_FRAME_HEIGHT(&frame),
                              GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0), QImage::Format_RGBA8888);
            m_lastFrame = view.copy(); // copia antes de liberar el mapeo
            gst_video_frame_unmap(&frame);
            ++m_frameCount;
            m_sinceLastFrame.restart();
            if (!m_hasSignal) {
                m_hasSignal = true;
                emit signalChanged(true);
            }
            emit frameReady(m_lastFrame);
        }
        gst_sample_unref(sample);
    }

    if (m_hasSignal && m_sinceLastFrame.elapsed() > m_signalTimeoutMs) {
        m_hasSignal = false;
        m_lastFrame = QImage(); // no mostrar una imagen congelada como si fuera en directo
        emit signalChanged(false);
    }
}

} // namespace lanwindow
