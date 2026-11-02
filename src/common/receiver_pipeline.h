#pragma once

#include "common/config.h"

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QTimer>

#include <gst/gst.h>

namespace lanwindow {

// Pipeline del receptor: UDP -> jitterbuffer -> depay -> parse -> avdec_h264 -> RGBA -> appsink.
// Las muestras y el bus se sondean desde un QTimer preciso, igual en Linux y Windows.
class ReceiverPipeline : public QObject {
    Q_OBJECT
public:
    explicit ReceiverPipeline(QObject *parent = nullptr);
    ~ReceiverPipeline() override;

    bool start(const ReceiverConfig &cfg, QString *error);
    void stop();
    bool isRunning() const { return m_pipeline != nullptr; }
    int boundPort() const { return m_boundPort; }
    bool hasSignal() const { return m_hasSignal; }
    quint64 frameCount() const { return m_frameCount; }
    QImage lastFrame() const { return m_lastFrame; }

signals:
    void frameReady(const QImage &frame);
    void signalChanged(bool hasSignal); // false: no llegan fotogramas desde hace signalTimeoutMs
    void failed(const QString &message); // ya detenido al emitirse
    void warning(const QString &message);

private:
    void poll();

    GstElement *m_pipeline = nullptr;
    GstElement *m_appsink = nullptr; // propiedad del pipeline
    QTimer m_pollTimer;
    QElapsedTimer m_sinceLastFrame;
    QImage m_lastFrame;
    quint64 m_frameCount = 0;
    int m_boundPort = 0;
    int m_signalTimeoutMs = 3000;
    bool m_hasSignal = false;
};

} // namespace lanwindow
