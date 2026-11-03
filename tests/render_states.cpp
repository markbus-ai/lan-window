// Herramienta de revisión visual (no es un test): guarda capturas de los estados reales de las ventanas.
// Uso: QT_QPA_PLATFORM=offscreen ui_render_states <carpeta-de-salida>
#include "common/theme.h"
#include "receiver/receiver_window.h"
#include "sender/sender_window.h"

#include <QApplication>
#include <QDir>
#include <QTest>

using namespace lanwindow;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (argc != 2)
        return 2;
    const QString out = QString::fromLocal8Bit(argv[1]);
    QDir().mkpath(out);
    QApplication::setOrganizationName(QStringLiteral("lan-window-render"));
    applyTheme(app);

    const auto save = [&](QWidget &window, const char *name) {
        QTest::qWait(150);
        window.grab().save(out + QLatin1Char('/') + QString::fromLatin1(name) + QStringLiteral(".png"));
    };
    const auto waitFor = [](const QWidget &, const auto &done) {
        for (int i = 0; i < 100 && !done(); ++i)
            QTest::qWait(100);
    };

    const int port = 5917;
    SenderWindow sender;
    ReceiverWindow receiver;
    sender.setDestination(QString(), port);
    sender.show();
    save(sender, "sender-empty");
    sender.setTestPattern(true);
    sender.setDestination(QStringLiteral("192.168.1.50"), port);
    receiver.show();
    save(sender, "sender-ready");
    save(receiver, "receiver-stopped");

    sender.setDestination(QStringLiteral("192.168.1.999"), port);
    sender.startSending();
    save(sender, "sender-error");

    receiver.setListen(QStringLiteral("999.0.0.1"), port);
    receiver.startReceiving();
    save(receiver, "receiver-error");

    receiver.setListen(QStringLiteral("127.0.0.1"), port);
    receiver.startReceiving();
    save(receiver, "receiver-waiting");

    sender.setDestination(QStringLiteral("127.0.0.1"), port);
    sender.startSending();
    save(sender, "sender-live");
    waitFor(receiver, [&] { return receiver.statusText().startsWith(QStringLiteral("En directo")); });
    save(receiver, "receiver-live");
    receiver.setControlsVisible(false);
    save(receiver, "receiver-hidden-controls");
    receiver.setControlsVisible(true);

    sender.stopSending();
    waitFor(receiver, [&] { return receiver.statusText().startsWith(QStringLiteral("Sin señal")); });
    save(receiver, "receiver-no-signal");
    save(sender, "sender-stopped");
    receiver.stopReceiving();
    return 0;
}
