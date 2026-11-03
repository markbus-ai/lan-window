#include "common/gst_util.h"
#include "common/theme.h"
#include "sender/sender_window.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("lan-window"));
    QApplication::setApplicationName(QStringLiteral("lan-window-sender"));
    lanwindow::ensureGstInit();
    lanwindow::applyTheme(app);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Emisor de pantalla (PipeWire/portal) en H.264/RTP por la LAN."));
    parser.addHelpOption();
    QCommandLineOption host("host", QStringLiteral("IPv4 del PC receptor."), "ipv4");
    QCommandLineOption port("port", QStringLiteral("Puerto UDP de destino."), "puerto");
    QCommandLineOption quality("quality", QStringLiteral("Calidad: 720 o 1080."), "calidad");
    QCommandLineOption test("test-pattern", QStringLiteral("Enviar patrón de prueba en lugar de la pantalla."));
    QCommandLineOption autostart("autostart", QStringLiteral("Empezar a enviar al abrir."));
    QCommandLineOption selfCheck("self-check", QStringLiteral("Comprobar plugins y codificador, y salir (0 = correcto)."));
    parser.addOptions({host, port, quality, test, autostart, selfCheck});
    parser.process(app);

    if (parser.isSet(selfCheck)) {
        const QStringList missing = lanwindow::missingElements(lanwindow::requiredSenderElements(true));
        if (!missing.isEmpty())
            qCritical("Faltan plugins GStreamer: %s", qPrintable(missing.join(", ")));
        const QString encoder = lanwindow::pickH264Encoder();
        QString encoderError = QStringLiteral("No hay codificador H.264.");
        const bool encoderOk = !encoder.isEmpty() && lanwindow::probeEncoder(encoder, &encoderError);
        if (!encoderOk)
            qCritical("%s", qPrintable(encoderError));
        return missing.isEmpty() && encoderOk ? 0 : 1;
    }

    lanwindow::SenderWindow window;
    if (parser.isSet(host) || parser.isSet(port))
        window.setDestination(parser.isSet(host) ? parser.value(host) : QString(),
                              parser.isSet(port) ? parser.value(port).toInt() : lanwindow::kDefaultPort);
    if (parser.isSet(quality))
        window.setQualityIndex(parser.value(quality) == QLatin1String("1080") ? 1 : 0);
    if (parser.isSet(test))
        window.setTestPattern(true);
    window.show();
    if (parser.isSet(autostart))
        window.startSending();
    return app.exec();
}
