#include "common/sender_pipeline.h"

#include "common/gst_util.h"

#include <QThread>
#include <algorithm>
#include <gst/video/video.h>

namespace lanwindow {

namespace {

void setCaps(GstElement *capsfilter, const QString &capsText)
{
    GstCaps *caps = gst_caps_from_string(capsText.toUtf8().constData());
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);
}

// Cola de un solo búfer que descarta lo antiguo: bajo carga se pierden fotogramas, no se acumula latencia.
void makeLeaky(GstElement *queue)
{
    g_object_set(queue, "max-size-buffers", 1u, "max-size-bytes", 0u, "max-size-time", guint64(0), nullptr);
    gst_util_set_object_arg(G_OBJECT(queue), "leaky", "downstream");
}

// Limitador de fotogramas sin latencia (videorate retiene cada fotograma hasta que llega el siguiente):
// descarta los que llegan antes de 3/4 del intervalo objetivo desde el último aceptado.
struct RateLimit {
    GstClockTime minGap;
    GstClockTime last = GST_CLOCK_TIME_NONE;
};

GstPadProbeReturn rateLimitProbe(GstPad *, GstPadProbeInfo *info, gpointer data)
{
    auto *rl = static_cast<RateLimit *>(data);
    const GstClockTime pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    if (!GST_CLOCK_TIME_IS_VALID(pts))
        return GST_PAD_PROBE_OK;
    if (GST_CLOCK_TIME_IS_VALID(rl->last) && pts >= rl->last && pts - rl->last < rl->minGap)
        return GST_PAD_PROBE_DROP;
    rl->last = pts;
    return GST_PAD_PROBE_OK;
}

void configureEncoder(GstElement *enc, const QString &name, const SenderConfig &cfg)
{
    const int gop = std::max(1, cfg.fps * cfg.keyframeIntervalMs / 1000);
    const int threads = std::clamp(QThread::idealThreadCount(), 1, 4);
    if (name == QLatin1String("x264enc")) {
        g_object_set(enc, "bitrate", guint(cfg.bitrateKbps), "key-int-max", guint(gop), "bframes", 0u,
                     "threads", guint(threads), nullptr);
        gst_util_set_object_arg(G_OBJECT(enc), "tune", "zerolatency");
        gst_util_set_object_arg(G_OBJECT(enc), "speed-preset", "veryfast");
    } else { // openh264enc: tasa en bit/s
        g_object_set(enc, "bitrate", guint(cfg.bitrateKbps) * 1000u, "max-bitrate",
                     guint(cfg.bitrateKbps) * 1500u, "gop-size", guint(gop), "multi-thread", guint(threads),
                     "num-slices", guint(threads), nullptr); // openh264 solo paraleliza por slices
        gst_util_set_object_arg(G_OBJECT(enc), "usage-type", "screen");
        gst_util_set_object_arg(G_OBJECT(enc), "complexity", "low");
        gst_util_set_object_arg(G_OBJECT(enc), "rate-control", "bitrate");
        gst_util_set_object_arg(G_OBJECT(enc), "slice-mode", "n-slices");
    }
}

} // namespace

SenderPipeline::SenderPipeline(QObject *parent) : QObject(parent)
{
    m_busTimer.setInterval(50);
    connect(&m_busTimer, &QTimer::timeout, this, &SenderPipeline::pollBus);
    connect(&m_keyframeTimer, &QTimer::timeout, this, &SenderPipeline::forceKeyframe);
}

SenderPipeline::~SenderPipeline()
{
    stop();
}

bool SenderPipeline::start(const SenderConfig &cfg, QString *error)
{
    stop();
    ensureGstInit();
    auto fail = [&](const QString &msg) {
        destroyPipeline(&m_pipeline);
        m_encoder = nullptr;
        if (error)
            *error = msg;
        return false;
    };

    const QString invalid = validateSenderConfig(cfg);
    if (!invalid.isEmpty())
        return fail(invalid);
    const bool pw = cfg.source == SourceKind::PipeWire;
    const QStringList missing = missingElements(requiredSenderElements(pw));
    if (!missing.isEmpty())
        return fail(QStringLiteral("Faltan elementos GStreamer: %1").arg(missing.join(", ")));
    m_encoderName = pickH264Encoder();
    if (m_encoderName.isEmpty())
        return fail(QStringLiteral("No hay codificador H.264 (x264enc u openh264enc)."));

    QString err;
    m_pipeline = gst_pipeline_new("sender");
    std::vector<GstElement *> chain;
    auto add = [&](const char *factory, const char *name) -> GstElement * {
        GstElement *e = makeElement(factory, name, &err);
        if (e) {
            gst_bin_add(GST_BIN(m_pipeline), e);
            chain.push_back(e);
        }
        return e;
    };

    const QString scaledCaps = QStringLiteral("video/x-raw,format=I420,width=%1,height=%2,pixel-aspect-ratio=1/1")
                                   .arg(cfg.width)
                                   .arg(cfg.height);
    const QString rawCaps = scaledCaps + QStringLiteral(",framerate=%1/1").arg(cfg.fps);
    const int threads = std::clamp(QThread::idealThreadCount(), 1, 4);
    if (cfg.source != SourceKind::TestPattern) {
        // Cadena de captura: cualquier formato/tamaño/ritmo -> I420 al tamaño pedido con bandas negras.
        GstElement *src = add(pw ? "pipewiresrc" : "videotestsrc", "src");
        GstElement *sysmem = add("capsfilter", "sysmem");
        GstElement *q = add("queue", "srcqueue");
        GstElement *conv = add("videoconvert", "convert");
        GstElement *scale = add("videoscale", "scale");
        GstElement *caps = add("capsfilter", "rawcaps");
        if (!err.isEmpty())
            return fail(err);
        if (pw) {
            // "path" es el id de nodo PipeWire que entrega el portal (target-object espera nombre/serial).
            const QByteArray node = QByteArray::number(cfg.pipewireNodeId);
            g_object_set(src, "fd", cfg.pipewireFd, "path", node.constData(), "do-timestamp", TRUE, nullptr);
            // La captura de escritorio sólo entrega fotogramas cuando cambia la pantalla: reenviar el último.
            if (g_object_class_find_property(G_OBJECT_GET_CLASS(src), "keepalive-time"))
                g_object_set(src, "keepalive-time", 100, nullptr);
            if (g_object_class_find_property(G_OBJECT_GET_CLASS(src), "on-disconnect"))
                gst_util_set_object_arg(G_OBJECT(src), "on-disconnect", "eos");
            setCaps(sysmem, QStringLiteral("video/x-raw")); // memoria de sistema, no DMA-BUF
        } else { // SimulatedCapture (tests): BGRx 16:10 a 10 fps
            g_object_set(src, "is-live", TRUE, nullptr);
            gst_util_set_object_arg(G_OBJECT(src), "pattern", "smpte");
            setCaps(sysmem, QStringLiteral("video/x-raw,format=BGRx,width=1920,height=1200,framerate=60/1"));
        }
        makeLeaky(q);
        GstPad *qsink = gst_element_get_static_pad(q, "sink");
        gst_pad_add_probe(qsink, GST_PAD_PROBE_TYPE_BUFFER, rateLimitProbe,
                          new RateLimit{GST_SECOND * 3 / (4 * guint64(cfg.fps))},
                          [](gpointer d) { delete static_cast<RateLimit *>(d); });
        gst_object_unref(qsink);
        g_object_set(conv, "n-threads", guint(threads), nullptr);
        g_object_set(scale, "n-threads", guint(threads), "add-borders", TRUE, nullptr);
        setCaps(caps, scaledCaps); // ritmo variable: el de la captura, limitado por la sonda
    } else {
        GstElement *src = add("videotestsrc", "src");
        GstElement *caps = add("capsfilter", "rawcaps");
        if (!err.isEmpty())
            return fail(err);
        g_object_set(src, "is-live", TRUE, nullptr);
        if (cfg.pattern == TestPattern::SolidColor) {
            gst_util_set_object_arg(G_OBJECT(src), "pattern", "solid-color");
            g_object_set(src, "foreground-color", guint(cfg.solidColorArgb), nullptr);
        } else {
            gst_util_set_object_arg(G_OBJECT(src), "pattern", "smpte");
        }
        setCaps(caps, rawCaps);
    }

    GstElement *encQueue = add("queue", "encqueue");
    m_encoder = add(m_encoderName.toUtf8().constData(), "encoder");
    GstElement *h264caps = add("capsfilter", "h264caps");
    GstElement *parse = add("h264parse", "parse");
    GstElement *pay = add("rtph264pay", "pay");
    // Solo tests: "identity" tras el payloader simula pérdida de paquetes RTP.
    GstElement *drop = cfg.debugDropProbability > 0.0 ? add("identity", "drop") : nullptr;
    GstElement *sink = add("udpsink", "sink");
    if (!err.isEmpty())
        return fail(err);

    makeLeaky(encQueue);
    configureEncoder(m_encoder, m_encoderName, cfg);
    setCaps(h264caps, QStringLiteral("video/x-h264,stream-format=byte-stream,alignment=au"));
    g_object_set(parse, "config-interval", -1, nullptr); // SPS/PPS con cada IDR
    g_object_set(pay, "pt", guint(kRtpPayloadType), "mtu", guint(kRtpMtu), "config-interval", -1, nullptr);
    if (drop)
        g_object_set(drop, "drop-probability", gfloat(cfg.debugDropProbability), nullptr);
    const QByteArray host = cfg.host.toUtf8(); // validado como IPv4 estricta
    g_object_set(sink, "host", host.constData(), "port", cfg.port, "sync", FALSE, "async", FALSE, nullptr);

    for (size_t i = 0; i + 1 < chain.size(); ++i) {
        if (!gst_element_link(chain[i], chain[i + 1]))
            return fail(QStringLiteral("No se pudieron enlazar %1 y %2.")
                            .arg(QString::fromUtf8(GST_OBJECT_NAME(chain[i])),
                                 QString::fromUtf8(GST_OBJECT_NAME(chain[i + 1]))));
    }

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        QString msg = QStringLiteral("No se pudo iniciar el pipeline del emisor.");
        for (const BusEvent &ev : drainBus(m_pipeline))
            if (ev.type == BusEvent::Error)
                msg += QLatin1Char(' ') + ev.text;
        return fail(msg);
    }
    m_busTimer.start();
    m_keyframeTimer.start(cfg.keyframeIntervalMs);
    return true;
}

void SenderPipeline::stop()
{
    m_busTimer.stop();
    m_keyframeTimer.stop();
    m_encoder = nullptr;
    destroyPipeline(&m_pipeline);
}

void SenderPipeline::forceKeyframe()
{
    if (!m_encoder)
        return;
    GstPad *pad = gst_element_get_static_pad(m_encoder, "src");
    if (!pad)
        return;
    gst_pad_send_event(pad, gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 0));
    gst_object_unref(pad);
}

void SenderPipeline::pollBus()
{
    for (const BusEvent &ev : drainBus(m_pipeline)) {
        switch (ev.type) {
        case BusEvent::Warning:
            emit warning(ev.text);
            break;
        case BusEvent::Error:
            stop();
            emit failed(ev.text);
            return;
        case BusEvent::Eos:
            stop();
            emit finished();
            return;
        }
    }
}

} // namespace lanwindow
