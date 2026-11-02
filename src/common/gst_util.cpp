#include "common/gst_util.h"

#include <QCoreApplication>
#include <QDir>
#include <mutex>

namespace lanwindow {

void ensureGstInit()
{
    static std::once_flag once;
    std::call_once(once, [] {
#ifdef Q_OS_WIN
        // Paquete portable: los plugins viajan junto al ejecutable, no en la instalación de MSYS2.
        if (QCoreApplication::instance()) {
            const QString plugins = QCoreApplication::applicationDirPath() + QStringLiteral("/gstreamer-1.0");
            if (QDir(plugins).exists())
                qputenv("GST_PLUGIN_SYSTEM_PATH_1_0", QDir::toNativeSeparators(plugins).toLocal8Bit());
        }
#endif
        gst_init(nullptr, nullptr);
    });
}

QStringList requiredReceiverElements()
{
    return {"udpsrc", "rtpjitterbuffer", "rtph264depay", "h264parse", "avdec_h264",
            "videoconvert", "capsfilter", "appsink"};
}

QStringList requiredSenderElements(bool withPipeWire)
{
    QStringList list{"videotestsrc", "videoconvert", "videoscale", "capsfilter",
                     "queue", "h264parse", "rtph264pay", "udpsink"};
    if (withPipeWire)
        list.prepend("pipewiresrc");
    return list;
}

QStringList missingElements(const QStringList &factories)
{
    ensureGstInit();
    QStringList missing;
    for (const QString &name : factories) {
        GstElementFactory *f = gst_element_factory_find(name.toUtf8().constData());
        if (!f)
            missing << name;
        else
            gst_object_unref(f);
    }
    return missing;
}

QString pickH264Encoder()
{
    if (missingElements({"x264enc"}).isEmpty())
        return QStringLiteral("x264enc");
    if (missingElements({"openh264enc"}).isEmpty())
        return QStringLiteral("openh264enc");
    return {};
}

GstElement *makeElement(const char *factory, const char *name, QString *error)
{
    GstElement *e = gst_element_factory_make(factory, name);
    if (!e && error && error->isEmpty())
        *error = QStringLiteral("Falta el elemento GStreamer «%1».").arg(QString::fromUtf8(factory));
    return e;
}

static QString messageText(GstMessage *msg)
{
    GError *err = nullptr;
    gchar *dbg = nullptr;
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
        gst_message_parse_error(msg, &err, &dbg);
    else
        gst_message_parse_warning(msg, &err, &dbg);
    QString src = QString::fromUtf8(GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)));
    QString text = QStringLiteral("%1: %2").arg(src, err ? QString::fromUtf8(err->message) : QString());
    if (err)
        g_error_free(err);
    g_free(dbg);
    return text;
}

std::vector<BusEvent> drainBus(GstElement *pipeline)
{
    std::vector<BusEvent> out;
    if (!pipeline)
        return out;
    GstBus *bus = gst_element_get_bus(pipeline);
    const auto mask = GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING | GST_MESSAGE_EOS);
    while (GstMessage *msg = gst_bus_pop_filtered(bus, mask)) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            out.push_back({BusEvent::Error, messageText(msg)});
            break;
        case GST_MESSAGE_WARNING:
            out.push_back({BusEvent::Warning, messageText(msg)});
            break;
        default:
            out.push_back({BusEvent::Eos, QString()});
            break;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    return out;
}

void destroyPipeline(GstElement **pipeline)
{
    if (!*pipeline)
        return;
    gst_element_set_state(*pipeline, GST_STATE_NULL);
    gst_object_unref(*pipeline);
    *pipeline = nullptr;
}

bool probeEncoder(const QString &factory, QString *error)
{
    ensureGstInit();
    QString err;
    GstElement *pipeline = gst_pipeline_new("encoder-probe");
    GstElement *src = makeElement("videotestsrc", nullptr, &err);
    GstElement *caps = makeElement("capsfilter", nullptr, &err);
    GstElement *enc = makeElement(factory.toUtf8().constData(), nullptr, &err);
    GstElement *sink = makeElement("fakesink", nullptr, &err);
    if (!src || !caps || !enc || !sink) {
        for (GstElement *e : {src, caps, enc, sink})
            if (e)
                gst_object_unref(e);
        gst_object_unref(pipeline);
        if (error)
            *error = err;
        return false;
    }
    g_object_set(src, "num-buffers", 2, nullptr);
    GstCaps *c = gst_caps_from_string("video/x-raw,format=I420,width=64,height=64,framerate=30/1");
    g_object_set(caps, "caps", c, nullptr);
    gst_caps_unref(c);
    gst_bin_add_many(GST_BIN(pipeline), src, caps, enc, sink, nullptr);
    bool ok = gst_element_link_many(src, caps, enc, sink, nullptr);
    if (ok)
        ok = gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE;
    if (ok) {
        GstBus *bus = gst_element_get_bus(pipeline);
        GstMessage *msg = gst_bus_timed_pop_filtered(
            bus, 5 * GST_SECOND, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        ok = msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
        if (!msg)
            err = QStringLiteral("tiempo de espera agotado");
        else if (!ok)
            err = messageText(msg);
        if (msg)
            gst_message_unref(msg);
        gst_object_unref(bus);
    }
    destroyPipeline(&pipeline);
    if (!ok && error)
        *error = QStringLiteral("El codificador %1 no funciona: %2").arg(factory, err);
    return ok;
}

} // namespace lanwindow
