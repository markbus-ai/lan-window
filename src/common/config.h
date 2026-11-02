#pragma once

#include <QString>
#include <QtGlobal>

namespace lanwindow {

constexpr int kDefaultPort = 5004;
constexpr int kRtpPayloadType = 96;
constexpr int kRtpMtu = 1200;

// SimulatedCapture: videotestsrc BGRx 1920x1200 a 60 fps por la misma cadena que PipeWire (tests).
enum class SourceKind { TestPattern, PipeWire, SimulatedCapture };
enum class TestPattern { ColorBars, SolidColor };

struct QualityPreset {
    const char *label;
    int width;
    int height;
    int fps;
    int bitrateKbps;
};

// Calidades ofrecidas en la UI del emisor. El índice 0 es el predeterminado.
const QualityPreset *qualityPresets(int *count);

struct SenderConfig {
    QString host;                 // IPv4 unicast de destino (validada)
    int port = kDefaultPort;
    int width = 1280;
    int height = 720;
    int fps = 30;
    int bitrateKbps = 4000;
    int keyframeIntervalMs = 2000; // IDR forzado periódicamente (captura por daño)
    SourceKind source = SourceKind::TestPattern;
    TestPattern pattern = TestPattern::ColorBars;
    quint32 solidColorArgb = 0xffff0000;
    int pipewireFd = -1;          // propiedad del llamador; debe seguir abierto mientras corre
    quint32 pipewireNodeId = 0;
    double debugDropProbability = 0.0; // solo tests: pérdida RTP simulada
};

struct ReceiverConfig {
    QString bindAddress = QStringLiteral("0.0.0.0");
    int port = kDefaultPort;      // 0 = puerto libre automático (tests)
    int latencyMs = 80;
    int signalTimeoutMs = 3000;   // sin fotogramas durante este tiempo => "sin señal"
};

// Valida IPv4 en notación decimal con puntos estricta (a.b.c.d, 0-255, sin ceros a la izquierda).
bool isValidIPv4(const QString &text);

// Devuelven cadena vacía si es válida, o un mensaje de error en español.
QString validateSenderConfig(const SenderConfig &cfg);
QString validateReceiverConfig(const ReceiverConfig &cfg);

} // namespace lanwindow
