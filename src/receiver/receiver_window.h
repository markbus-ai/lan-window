#pragma once

#include "common/receiver_pipeline.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace lanwindow {

class VideoWidget;

// Ventana del receptor: controles arriba (ocultables con F11) y el vídeo debajo.
// Esta es la ventana que se selecciona en Discord.
class ReceiverWindow : public QWidget {
    Q_OBJECT
public:
    explicit ReceiverWindow(QWidget *parent = nullptr);

    void setListen(const QString &address, int port);
    bool startReceiving();
    void stopReceiving();
    void setControlsVisible(bool visible);

    ReceiverPipeline *pipeline() { return &m_pipeline; }
    VideoWidget *videoWidget() { return m_video; }
    QString statusText() const;

private:
    void setStatus(const QString &text, bool error = false);
    void updateButtons();
    void onSignalChanged(bool hasSignal);

    ReceiverPipeline m_pipeline;
    QWidget *m_controls = nullptr;
    QLineEdit *m_address = nullptr;
    QSpinBox *m_port = nullptr;
    QPushButton *m_startStop = nullptr;
    QLabel *m_status = nullptr;
    VideoWidget *m_video = nullptr;
    QString m_listenText;
    bool m_diagnosticsOk = true;
};

} // namespace lanwindow
