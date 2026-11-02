#include "common/gst_util.h"
#include "receiver/receiver_window.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("lan-window-receiver"));
    lanwindow::ensureGstInit();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Receptor de vídeo H.264/RTP para compartir en Discord."));
    parser.addHelpOption();
    QCommandLineOption bind("bind", QStringLiteral("IPv4 local de escucha (por defecto 0.0.0.0)."), "ipv4",
                            QStringLiteral("0.0.0.0"));
    QCommandLineOption port("port", QStringLiteral("Puerto UDP (por defecto 5004)."), "puerto",
                            QString::number(lanwindow::kDefaultPort));
    QCommandLineOption autostart("autostart", QStringLiteral("Empezar a recibir al abrir."));
    QCommandLineOption hide("hide-controls", QStringLiteral("Abrir con los controles ocultos (F11 los muestra)."));
    QCommandLineOption selfCheck("self-check", QStringLiteral("Comprobar los plugins necesarios y salir (0 = correcto)."));
    parser.addOptions({bind, port, autostart, hide, selfCheck});
    parser.process(app);

    if (parser.isSet(selfCheck)) {
        const QStringList missing = lanwindow::missingElements(lanwindow::requiredReceiverElements());
        if (!missing.isEmpty())
            qCritical("Faltan plugins GStreamer: %s", qPrintable(missing.join(", ")));
        return missing.isEmpty() ? 0 : 1;
    }

    lanwindow::ReceiverWindow window;
    window.setListen(parser.value(bind), parser.value(port).toInt());
    if (parser.isSet(hide))
        window.setControlsVisible(false);
    window.show();
    if (parser.isSet(autostart))
        window.startReceiving();
    return app.exec();
}
