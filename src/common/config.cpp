#include "common/config.h"

#include <QStringList>

namespace lanwindow {

const QualityPreset *qualityPresets(int *count)
{
    static const QualityPreset presets[] = {
        {"720p 30 fps (4 Mbit/s)", 1280, 720, 30, 4000},
        {"1080p 30 fps (8 Mbit/s)", 1920, 1080, 30, 8000},
    };
    if (count)
        *count = int(sizeof(presets) / sizeof(presets[0]));
    return presets;
}

bool isValidIPv4(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char('.'));
    if (parts.size() != 4)
        return false;
    for (const QString &p : parts) {
        if (p.isEmpty() || p.size() > 3)
            return false;
        for (QChar c : p) {
            if (c < QLatin1Char('0') || c > QLatin1Char('9'))
                return false;
        }
        if (p.size() > 1 && p.front() == QLatin1Char('0'))
            return false;
        if (p.toInt() > 255)
            return false;
    }
    return true;
}

QString validateSenderConfig(const SenderConfig &cfg)
{
    if (!isValidIPv4(cfg.host))
        return QStringLiteral("La dirección de destino debe ser una IPv4 válida (p. ej. 192.168.1.50).");
    if (cfg.host == QLatin1String("0.0.0.0") || cfg.host == QLatin1String("255.255.255.255"))
        return QStringLiteral("La dirección de destino debe ser la IPv4 concreta del PC receptor.");
    if (cfg.port < 1 || cfg.port > 65535)
        return QStringLiteral("El puerto debe estar entre 1 y 65535.");
    if (cfg.width < 16 || cfg.height < 16 || cfg.width > 7680 || cfg.height > 4320
        || cfg.width % 2 || cfg.height % 2)
        return QStringLiteral("Resolución no válida.");
    if (cfg.fps < 1 || cfg.fps > 120)
        return QStringLiteral("Fotogramas por segundo no válidos.");
    if (cfg.bitrateKbps < 100 || cfg.bitrateKbps > 100000)
        return QStringLiteral("Tasa de bits no válida.");
    if (cfg.keyframeIntervalMs < 200)
        return QStringLiteral("Intervalo de fotogramas clave demasiado corto.");
    if (cfg.source == SourceKind::PipeWire && cfg.pipewireFd < 0)
        return QStringLiteral("No hay sesión de captura de pantalla activa.");
    if (cfg.debugDropProbability < 0.0 || cfg.debugDropProbability >= 1.0)
        return QStringLiteral("Probabilidad de pérdida no válida.");
    return {};
}

QString validateReceiverConfig(const ReceiverConfig &cfg)
{
    if (!isValidIPv4(cfg.bindAddress))
        return QStringLiteral("La dirección de escucha debe ser una IPv4 válida (0.0.0.0 = todas las interfaces).");
    if (cfg.port < 0 || cfg.port > 65535)
        return QStringLiteral("El puerto debe estar entre 1 y 65535.");
    if (cfg.latencyMs < 0 || cfg.latencyMs > 5000)
        return QStringLiteral("Latencia del búfer no válida.");
    if (cfg.signalTimeoutMs < 100)
        return QStringLiteral("Tiempo de espera de señal no válido.");
    return {};
}

} // namespace lanwindow
