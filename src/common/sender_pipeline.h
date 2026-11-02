#pragma once

#include "common/config.h"

#include <QObject>
#include <QTimer>

#include <gst/gst.h>

namespace lanwindow {

// Pipeline del emisor: fuente (videotestsrc o pipewiresrc) -> I420 -> H.264 -> RTP -> UDP.
// El bus se sondea desde un QTimer (no depende de un bucle GLib).
class SenderPipeline : public QObject {
    Q_OBJECT
public:
    explicit SenderPipeline(QObject *parent = nullptr);
    ~SenderPipeline() override;

    bool start(const SenderConfig &cfg, QString *error);
    void stop();
    bool isRunning() const { return m_pipeline != nullptr; }
    QString encoderName() const { return m_encoderName; }

signals:
    void failed(const QString &message); // el pipeline ya se ha detenido al emitirse
    void warning(const QString &message);
    void finished();                     // EOS (p. ej. la captura terminó); ya detenido

private:
    void pollBus();
    void forceKeyframe();

    GstElement *m_pipeline = nullptr;
    GstElement *m_encoder = nullptr; // propiedad del pipeline
    QTimer m_busTimer;
    QTimer m_keyframeTimer;
    QString m_encoderName;
};

} // namespace lanwindow
