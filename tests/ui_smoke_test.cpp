// Smoke test de la UI con QT_QPA_PLATFORM=offscreen: ventanas reales de emisor (patrón de prueba,
// nunca el portal) y receptor, conectadas por UDP en 127.0.0.1.
#include "common/receiver_pipeline.h"
#include "receiver/receiver_window.h"
#include "receiver/video_widget.h"
#include "sender/sender_window.h"

#include <QApplication>
#include <QLineEdit>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace lanwindow;

namespace {

int freeUdpPort()
{
    ReceiverPipeline probe;
    ReceiverConfig cfg;
    cfg.bindAddress = QStringLiteral("127.0.0.1");
    cfg.port = 0;
    QString err;
    if (!probe.start(cfg, &err))
        return -1;
    return probe.boundPort();
}

// Color que pinta el widget en la fracción fx del ancho de la imagen y fy del alto.
QColor widgetColorAt(VideoWidget *w, double fx, double fy)
{
    const QImage shot = w->grab().toImage();
    const QSize scaled = w->frame().size().scaled(w->size(), Qt::KeepAspectRatio);
    const int x0 = (w->width() - scaled.width()) / 2, y0 = (w->height() - scaled.height()) / 2;
    const qreal dpr = shot.devicePixelRatio();
    return shot.pixelColor(int((x0 + fx * scaled.width()) * dpr), int((y0 + fy * scaled.height()) * dpr));
}

} // namespace

class UiSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void senderRejectsInvalidHost();
    void returnKeyStartsSending();
    void portFieldShowsValueAndFallsBack();
    void endToEndWindows();
};

void UiSmokeTest::portFieldShowsValueAndFallsBack()
{
    PortField field;
    field.resize(120, 40);
    field.setValue(5917);
    QCOMPARE(field.text(), QStringLiteral("5917"));
    QCOMPARE(field.value(), 5917);
    field.clear();
    QCOMPARE(field.value(), kDefaultPort);
    field.setText(QStringLiteral("0"));
    QCOMPARE(field.value(), kDefaultPort);
    field.setValue(70000);
    QCOMPARE(field.value(), kDefaultPort);
}

void UiSmokeTest::senderRejectsInvalidHost()
{
    SenderWindow sender;
    sender.setTestPattern(true);
    sender.setDestination(QStringLiteral("999.1.1.1"), 5004);
    QVERIFY(!sender.startSending());
    QVERIFY(!sender.pipeline()->isRunning());
    QVERIFY(sender.statusText().startsWith(QStringLiteral("Error")));
    auto *host = sender.findChild<QLineEdit *>(QStringLiteral("hostField"));
    QVERIFY(host);
    QVERIFY(host->property("invalid").toBool());
    QCOMPARE(sender.focusWidget(), host);
    QTest::keyClick(host, Qt::Key_1);
    QVERIFY(!host->property("invalid").toBool());
}

void UiSmokeTest::returnKeyStartsSending()
{
    SenderWindow sender;
    sender.setTestPattern(true);
    sender.setDestination(QStringLiteral("999.1.1.1"), 5004);
    auto *host = sender.findChild<QLineEdit *>(QStringLiteral("hostField"));
    QVERIFY(host);
    QTest::keyClick(host, Qt::Key_Return);
    QVERIFY(sender.statusText().startsWith(QStringLiteral("Error")));
}

void UiSmokeTest::endToEndWindows()
{
    ReceiverWindow receiver;
    receiver.resize(1100, 700);
    receiver.show();
    SenderWindow sender;
    sender.show();
    QVERIFY(!sender.statusText().contains(QStringLiteral("Falta")));

    for (int round = 0; round < 2; ++round) {
        const int port = freeUdpPort();
        QVERIFY(port > 0);
        receiver.setListen(QStringLiteral("127.0.0.1"), port);
        QVERIFY(receiver.startReceiving());
        QVERIFY(receiver.statusText().contains(QStringLiteral("esperando")));
        QVERIFY(!receiver.videoWidget()->placeholder().contains(QLatin1Char('.'))); // sin dirección IP en el vídeo

        sender.setTestPattern(true); // nunca abrir el selector real del portal en tests
        sender.setDestination(QStringLiteral("127.0.0.1"), port);
        QVERIFY(sender.startSending());
        QVERIFY(sender.statusText().contains(QStringLiteral("Enviando patrón de prueba")));

        QTRY_VERIFY_WITH_TIMEOUT(receiver.pipeline()->frameCount() >= 10, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(receiver.statusText().startsWith(QStringLiteral("En directo")), 5000);
        QCOMPARE(receiver.videoWidget()->frame().size(), QSize(1280, 720));

        receiver.setControlsVisible(round == 0); // el vídeo se sigue pintando sin controles
        QTest::qWait(100);
        const QColor yellow = widgetColorAt(receiver.videoWidget(), 1.5 / 7, 0.3);
        const QColor blue = widgetColorAt(receiver.videoWidget(), 6.5 / 7, 0.3);
        QVERIFY2(yellow.red() > 140 && yellow.green() > 140 && yellow.blue() < 70, qPrintable(yellow.name()));
        QVERIFY2(blue.red() < 70 && blue.green() < 70 && blue.blue() > 140, qPrintable(blue.name()));

        sender.stopSending();
        QVERIFY(!sender.pipeline()->isRunning());
        QTRY_VERIFY_WITH_TIMEOUT(receiver.statusText().startsWith(QStringLiteral("Sin señal")), 6000);
        QVERIFY(receiver.videoWidget()->frame().isNull());
        receiver.stopReceiving();
        QCOMPARE(receiver.statusText(), QStringLiteral("Detenido"));
    }
}

int main(int argc, char **argv)
{
    // No tocar la configuración real del usuario (QSettings del emisor).
    QTemporaryDir config;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QStandardPaths::setTestModeEnabled(true);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("lan-window-test"));
    UiSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ui_smoke_test.moc"
