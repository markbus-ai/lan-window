// Tests de extremo a extremo con GStreamer real:
// videotestsrc -> H.264 -> RTP/UDP 127.0.0.1 -> udpsrc -> avdec_h264 -> appsink -> QImage.
#include "common/config.h"
#include "common/gst_util.h"
#include "common/receiver_pipeline.h"
#include "common/sender_pipeline.h"

#include <QSignalSpy>
#include <QTest>

using namespace lanwindow;

namespace {

struct Rgb {
    int r, g, b;
};

Rgb pixelAt(const QImage &img, double fx, double fy)
{
    const QColor c = img.pixelColor(int(fx * (img.width() - 1)), int(fy * (img.height() - 1)));
    return {c.red(), c.green(), c.blue()};
}

QString describe(const Rgb &p)
{
    return QStringLiteral("(%1,%2,%3)").arg(p.r).arg(p.g).arg(p.b);
}

bool hi(int v) { return v > 140; }
bool lo(int v) { return v < 70; }

SenderConfig loopbackSender(int port)
{
    SenderConfig cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = port;
    cfg.keyframeIntervalMs = 1000;
    return cfg;
}

ReceiverConfig autoPortReceiver()
{
    ReceiverConfig cfg;
    cfg.bindAddress = QStringLiteral("127.0.0.1");
    cfg.port = 0; // el sistema asigna un puerto libre
    return cfg;
}

} // namespace

class LoopbackTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void ipv4Validation();
    void configValidation();
    void diagnostics();
    void colorBars720();
    void solidColor1080();
    void captureChainLetterboxAndRate();
    void startStopRestart();
    void signalLossAndRecovery();
    void portInUse();
    void nonLocalBindAddress();
    void packetLossRecovers();
};

void LoopbackTest::initTestCase()
{
    ensureGstInit();
}

void LoopbackTest::ipv4Validation()
{
    for (const char *ok : {"127.0.0.1", "192.168.1.50", "0.0.0.0", "255.255.255.255", "10.0.0.1"})
        QVERIFY2(isValidIPv4(QString::fromLatin1(ok)), ok);
    for (const char *bad : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "01.2.3.4", "1.2.3.-4", " 1.2.3.4",
                            "1.2.3.4 ", "a.b.c.d", "::1", "host.local", "1.2.3.4;rm", "1..3.4", "1.2.3.4/24"})
        QVERIFY2(!isValidIPv4(QString::fromLatin1(bad)), bad);
}

void LoopbackTest::configValidation()
{
    SenderConfig s = loopbackSender(5004);
    QVERIFY(validateSenderConfig(s).isEmpty());
    s.host = QStringLiteral("0.0.0.0");
    QVERIFY(!validateSenderConfig(s).isEmpty());
    s = loopbackSender(0);
    QVERIFY(!validateSenderConfig(s).isEmpty());
    s = loopbackSender(70000);
    QVERIFY(!validateSenderConfig(s).isEmpty());
    s = loopbackSender(5004);
    s.width = 1281;
    QVERIFY(!validateSenderConfig(s).isEmpty());
    s = loopbackSender(5004);
    s.source = SourceKind::PipeWire; // sin fd
    QVERIFY(!validateSenderConfig(s).isEmpty());

    ReceiverConfig r;
    QVERIFY(validateReceiverConfig(r).isEmpty());
    r.bindAddress = QStringLiteral("localhost");
    QVERIFY(!validateReceiverConfig(r).isEmpty());
    r = ReceiverConfig();
    r.port = 65536;
    QVERIFY(!validateReceiverConfig(r).isEmpty());

    // Los pipelines rechazan configuraciones inválidas sin crear nada.
    SenderPipeline sender;
    QString err;
    s = loopbackSender(5004);
    s.host = QStringLiteral("192.168.1.300");
    QVERIFY(!sender.start(s, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!sender.isRunning());
    ReceiverPipeline receiver;
    err.clear();
    r = ReceiverConfig();
    r.bindAddress = QStringLiteral("1.2.3.4; evil");
    QVERIFY(!receiver.start(r, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!receiver.isRunning());
}

void LoopbackTest::diagnostics()
{
    QCOMPARE(missingElements(requiredReceiverElements()), QStringList());
    QCOMPARE(missingElements(requiredSenderElements(false)), QStringList());
    QCOMPARE(missingElements({"lanwindow_elemento_inexistente"}), QStringList{"lanwindow_elemento_inexistente"});
    const QString enc = pickH264Encoder();
    QVERIFY(!enc.isEmpty());
    QString err;
    QVERIFY2(probeEncoder(enc, &err), qPrintable(err));
    QVERIFY(!probeEncoder(QStringLiteral("lanwindow_elemento_inexistente"), &err));
    qInfo("Codificador H.264 usado: %s", qPrintable(enc));
}

void LoopbackTest::colorBars720()
{
    ReceiverPipeline receiver;
    QString err;
    QVERIFY2(receiver.start(autoPortReceiver(), &err), qPrintable(err));
    QVERIFY(receiver.boundPort() > 0);
    QSignalSpy frames(&receiver, &ReceiverPipeline::frameReady);

    SenderPipeline sender;
    QVERIFY2(sender.start(loopbackSender(receiver.boundPort()), &err), qPrintable(err));
    QTRY_VERIFY_WITH_TIMEOUT(receiver.frameCount() >= 10, 15000);
    QVERIFY(receiver.hasSignal());
    QVERIFY(frames.count() >= 10);

    const QImage img = receiver.lastFrame();
    QCOMPARE(img.size(), QSize(1280, 720));
    // Barras SMPTE (75 %): blanco, amarillo, cian, verde, magenta, rojo, azul.
    const double y = 0.3;
    const Rgb white = pixelAt(img, 0.5 / 7, y), yellow = pixelAt(img, 1.5 / 7, y), cyan = pixelAt(img, 2.5 / 7, y),
              green = pixelAt(img, 3.5 / 7, y), magenta = pixelAt(img, 4.5 / 7, y), red = pixelAt(img, 5.5 / 7, y),
              blue = pixelAt(img, 6.5 / 7, y);
    QVERIFY2(hi(white.r) && hi(white.g) && hi(white.b), qPrintable(describe(white)));
    QVERIFY2(hi(yellow.r) && hi(yellow.g) && lo(yellow.b), qPrintable(describe(yellow)));
    QVERIFY2(lo(cyan.r) && hi(cyan.g) && hi(cyan.b), qPrintable(describe(cyan)));
    QVERIFY2(lo(green.r) && hi(green.g) && lo(green.b), qPrintable(describe(green)));
    QVERIFY2(hi(magenta.r) && lo(magenta.g) && hi(magenta.b), qPrintable(describe(magenta)));
    QVERIFY2(hi(red.r) && lo(red.g) && lo(red.b), qPrintable(describe(red)));
    QVERIFY2(lo(blue.r) && lo(blue.g) && hi(blue.b), qPrintable(describe(blue)));

    sender.stop();
    receiver.stop();
    QVERIFY(!receiver.isRunning());
    QVERIFY(receiver.lastFrame().isNull());
}

void LoopbackTest::solidColor1080()
{
    ReceiverPipeline receiver;
    QString err;
    QVERIFY2(receiver.start(autoPortReceiver(), &err), qPrintable(err));
    SenderConfig cfg = loopbackSender(receiver.boundPort());
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.bitrateKbps = 8000;
    cfg.pattern = TestPattern::SolidColor;
    cfg.solidColorArgb = 0xffff0000; // rojo
    SenderPipeline sender;
    QVERIFY2(sender.start(cfg, &err), qPrintable(err));
    QTRY_VERIFY_WITH_TIMEOUT(receiver.frameCount() >= 5, 15000);
    const QImage img = receiver.lastFrame();
    QCOMPARE(img.size(), QSize(1920, 1080));
    for (double fx : {0.05, 0.5, 0.95})
        for (double fy : {0.05, 0.5, 0.95}) {
            const Rgb p = pixelAt(img, fx, fy);
            QVERIFY2(p.r > 200 && p.g < 50 && p.b < 50, qPrintable(describe(p)));
        }
}

void LoopbackTest::captureChainLetterboxAndRate()
{
    // Misma cadena que la captura PipeWire (convert/scale/limitador) con una fuente 16:10 a 60 fps.
    ReceiverPipeline receiver;
    QString err;
    QVERIFY2(receiver.start(autoPortReceiver(), &err), qPrintable(err));
    SenderConfig cfg = loopbackSender(receiver.boundPort());
    cfg.source = SourceKind::SimulatedCapture;
    SenderPipeline sender;
    QVERIFY2(sender.start(cfg, &err), qPrintable(err));
    QTRY_VERIFY_WITH_TIMEOUT(receiver.frameCount() >= 5, 15000);
    const quint64 before = receiver.frameCount();
    QTest::qWait(2000);
    const quint64 got = receiver.frameCount() - before;
    qInfo("Captura simulada a 60 fps limitada a 30 -> %llu fotogramas recibidos en 2 s", got);
    // Límite superior: el limitador no deja pasar 60 fps. El inferior es laxo porque openh264
    // depende de la CPU (en un host cargado puede no llegar a 30 fps).
    QVERIFY2(got >= 10 && got <= 75, qPrintable(QStringLiteral("%1 fotogramas en 2 s").arg(got)));

    const QImage img = receiver.lastFrame();
    QCOMPARE(img.size(), QSize(1280, 720));
    // 1920x1200 -> 1152x720 centrado: bandas negras de 64 px a izquierda y derecha.
    for (int x : {8, 56, 1223, 1271}) {
        const QColor c = img.pixelColor(x, 200);
        QVERIFY2(c.red() < 40 && c.green() < 40 && c.blue() < 40, qPrintable(QStringLiteral("x=%1 %2").arg(x).arg(c.name())));
    }
    const QColor yellow = img.pixelColor(64 + int(1152 * 1.5 / 7), 200);
    const QColor blue = img.pixelColor(64 + int(1152 * 6.5 / 7), 200);
    QVERIFY2(yellow.red() > 140 && yellow.green() > 140 && yellow.blue() < 70, qPrintable(yellow.name()));
    QVERIFY2(blue.red() < 70 && blue.green() < 70 && blue.blue() > 140, qPrintable(blue.name()));
}

void LoopbackTest::startStopRestart()
{
    ReceiverPipeline receiver;
    SenderPipeline sender;
    QString err;
    for (int round = 0; round < 3; ++round) {
        QVERIFY2(receiver.start(autoPortReceiver(), &err), qPrintable(err));
        QVERIFY2(sender.start(loopbackSender(receiver.boundPort()), &err), qPrintable(err));
        QTRY_VERIFY_WITH_TIMEOUT(receiver.frameCount() >= 5, 15000);
        // Reiniciar solo el emisor mientras el receptor sigue escuchando.
        sender.stop();
        QVERIFY(!sender.isRunning());
        const quint64 before = receiver.frameCount();
        QVERIFY2(sender.start(loopbackSender(receiver.boundPort()), &err), qPrintable(err));
        QTRY_VERIFY_WITH_TIMEOUT(receiver.frameCount() >= before + 5, 15000);
        sender.stop();
        receiver.stop();
        QVERIFY(!receiver.isRunning());
        const quint64 stopped = receiver.frameCount();
        QTest::qWait(200);
        QCOMPARE(receiver.frameCount(), stopped); // nada llega tras detener
    }
}

void LoopbackTest::signalLossAndRecovery()
{
    ReceiverConfig rcfg = autoPortReceiver();
    rcfg.signalTimeoutMs = 500;
    ReceiverPipeline receiver;
    QSignalSpy changes(&receiver, &ReceiverPipeline::signalChanged);
    QString err;
    QVERIFY2(receiver.start(rcfg, &err), qPrintable(err));
    SenderPipeline sender;
    QVERIFY2(sender.start(loopbackSender(receiver.boundPort()), &err), qPrintable(err));
    QTRY_VERIFY_WITH_TIMEOUT(receiver.hasSignal(), 15000);

    sender.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!receiver.hasSignal(), 5000);
    QVERIFY(receiver.isRunning());
    QVERIFY(receiver.lastFrame().isNull()); // no queda una imagen congelada como "en directo"
    QVERIFY(changes.count() >= 2);
    QCOMPARE(changes.last().at(0).toBool(), false);

    QVERIFY2(sender.start(loopbackSender(receiver.boundPort()), &err), qPrintable(err));
    QTRY_VERIFY_WITH_TIMEOUT(receiver.hasSignal(), 15000);
    QCOMPARE(receiver.lastFrame().size(), QSize(1280, 720));
}

void LoopbackTest::portInUse()
{
    ReceiverPipeline first;
    QString err;
    QVERIFY2(first.start(autoPortReceiver(), &err), qPrintable(err));
    ReceiverConfig cfg = autoPortReceiver();
    cfg.port = first.boundPort();
    ReceiverPipeline second;
    err.clear();
    QVERIFY(!second.start(cfg, &err));
    QVERIFY(!second.isRunning());
    QVERIFY(err.contains(QString::number(cfg.port)));
    qInfo("Error esperado: %s", qPrintable(err));
}

void LoopbackTest::nonLocalBindAddress()
{
    ReceiverConfig cfg;
    cfg.bindAddress = QStringLiteral("192.0.2.1"); // TEST-NET-1, no asignada a este host
    cfg.port = 0;
    ReceiverPipeline receiver;
    QString err;
    QVERIFY(!receiver.start(cfg, &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!receiver.isRunning());
}

void LoopbackTest::packetLossRecovers()
{
    // 0,5 % de pérdida RTP aleatoria: el receptor descarta lo dañado hasta el siguiente IDR
    // y nunca muestra fotogramas corruptos (las barras deben verse correctas en todos).
    ReceiverPipeline receiver;
    QString err;
    QVERIFY2(receiver.start(autoPortReceiver(), &err), qPrintable(err));
    SenderConfig cfg = loopbackSender(receiver.boundPort());
    cfg.keyframeIntervalMs = 500;
    cfg.debugDropProbability = 0.005;
    // Las barras SMPTE llevan ruido animado (~25 paquetes RTP por fotograma P): la pérdida sí afecta.
    int badFrames = 0;
    connect(&receiver, &ReceiverPipeline::frameReady, this, [&badFrames](const QImage &img) {
        const Rgb yellow = pixelAt(img, 1.5 / 7, 0.3), blue = pixelAt(img, 6.5 / 7, 0.3);
        if (!(hi(yellow.r) && hi(yellow.g) && lo(yellow.b) && lo(blue.r) && lo(blue.g) && hi(blue.b)))
            ++badFrames;
    });
    SenderPipeline sender;
    QVERIFY2(sender.start(cfg, &err), qPrintable(err));
    QTRY_VERIFY_WITH_TIMEOUT(receiver.frameCount() >= 5, 15000);
    const quint64 before = receiver.frameCount();
    QTest::qWait(3000);
    const quint64 got = receiver.frameCount() - before;
    qInfo("Fotogramas decodificados en 3 s con 0,5 %% de pérdida: %llu de ~90", got);
    // Cada pérdida congela la imagen hasta el siguiente IDR; con IDR cada 500 ms debe recuperarse varias veces.
    QVERIFY2(got >= 8, qPrintable(QStringLiteral("solo %1 fotogramas en 3 s con pérdida").arg(got)));
    QCOMPARE(badFrames, 0);
    QVERIFY(receiver.isRunning());
}

QTEST_GUILESS_MAIN(LoopbackTest)
#include "loopback_test.moc"
