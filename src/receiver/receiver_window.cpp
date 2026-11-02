#include "receiver/receiver_window.h"

#include "common/gst_util.h"
#include "receiver/video_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QVBoxLayout>

namespace lanwindow {

ReceiverWindow::ReceiverWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("LAN Window – Receptor"));
    resize(1280, 760);

    m_controls = new QWidget(this);
    auto *row = new QHBoxLayout(m_controls);
    row->setContentsMargins(8, 6, 8, 6);
    m_address = new QLineEdit(QStringLiteral("0.0.0.0"), m_controls);
    m_address->setToolTip(QStringLiteral("IPv4 local donde escuchar. 0.0.0.0 = todas las interfaces."));
    m_address->setMaximumWidth(140);
    m_port = new QSpinBox(m_controls);
    m_port->setRange(1, 65535);
    m_port->setValue(kDefaultPort);
    m_startStop = new QPushButton(m_controls);
    auto *hide = new QPushButton(QStringLiteral("Ocultar controles (F11)"), m_controls);
    m_status = new QLabel(m_controls);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(new QLabel(QStringLiteral("Escuchar en:"), m_controls));
    row->addWidget(m_address);
    row->addWidget(new QLabel(QStringLiteral("Puerto UDP:"), m_controls));
    row->addWidget(m_port);
    row->addWidget(m_startStop);
    row->addWidget(m_status, 1);
    row->addWidget(hide);

    m_video = new VideoWidget(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_controls);
    layout->addWidget(m_video, 1);

    connect(m_startStop, &QPushButton::clicked, this, [this] {
        if (m_pipeline.isRunning())
            stopReceiving();
        else
            startReceiving();
    });
    connect(hide, &QPushButton::clicked, this, [this] { setControlsVisible(false); });
    auto *toggle = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(toggle, &QShortcut::activated, this, [this] { setControlsVisible(!m_controls->isVisible()); });
    auto *show = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(show, &QShortcut::activated, this, [this] { setControlsVisible(true); });

    connect(&m_pipeline, &ReceiverPipeline::frameReady, m_video, &VideoWidget::setFrame);
    connect(&m_pipeline, &ReceiverPipeline::signalChanged, this, &ReceiverWindow::onSignalChanged);
    connect(&m_pipeline, &ReceiverPipeline::failed, this, [this](const QString &msg) {
        m_video->setPlaceholder(QStringLiteral("Recepción detenida por un error."));
        setStatus(QStringLiteral("Error: %1").arg(msg), true);
        updateButtons();
    });
    connect(&m_pipeline, &ReceiverPipeline::warning, this,
            [](const QString &msg) { qWarning("Aviso GStreamer: %s", qPrintable(msg)); });

    const QStringList missing = missingElements(requiredReceiverElements());
    m_diagnosticsOk = missing.isEmpty();
    if (m_diagnosticsOk) {
        m_video->setPlaceholder(QStringLiteral("Detenido. Pulsa «Iniciar» para recibir."));
        setStatus(QStringLiteral("Detenido"));
    } else {
        const QString msg = QStringLiteral("Faltan plugins GStreamer: %1").arg(missing.join(", "));
        m_video->setPlaceholder(msg);
        setStatus(msg, true);
    }
    updateButtons();
}

void ReceiverWindow::setListen(const QString &address, int port)
{
    m_address->setText(address);
    m_port->setValue(port);
}

bool ReceiverWindow::startReceiving()
{
    if (!m_diagnosticsOk)
        return false;
    ReceiverConfig cfg;
    cfg.bindAddress = m_address->text().trimmed();
    cfg.port = m_port->value();
    QString error;
    if (!m_pipeline.start(cfg, &error)) {
        setStatus(QStringLiteral("Error: %1").arg(error), true);
        updateButtons();
        return false;
    }
    m_listenText = QStringLiteral("%1:%2").arg(cfg.bindAddress).arg(m_pipeline.boundPort());
    m_video->setPlaceholder(QStringLiteral("Esperando vídeo en %1…").arg(m_listenText));
    setStatus(QStringLiteral("Escuchando en %1 – esperando vídeo").arg(m_listenText));
    updateButtons();
    return true;
}

void ReceiverWindow::stopReceiving()
{
    m_pipeline.stop();
    m_video->setPlaceholder(QStringLiteral("Detenido. Pulsa «Iniciar» para recibir."));
    setStatus(QStringLiteral("Detenido"));
    updateButtons();
}

void ReceiverWindow::setControlsVisible(bool visible)
{
    m_controls->setVisible(visible);
}

QString ReceiverWindow::statusText() const
{
    return m_status->text();
}

void ReceiverWindow::setStatus(const QString &text, bool error)
{
    m_status->setText(text);
    m_status->setStyleSheet(error ? QStringLiteral("color: #d33;") : QString());
}

void ReceiverWindow::updateButtons()
{
    const bool running = m_pipeline.isRunning();
    m_startStop->setText(running ? QStringLiteral("Detener") : QStringLiteral("Iniciar"));
    m_startStop->setEnabled(m_diagnosticsOk);
    m_address->setEnabled(!running);
    m_port->setEnabled(!running);
}

void ReceiverWindow::onSignalChanged(bool hasSignal)
{
    if (!m_pipeline.isRunning())
        return;
    if (hasSignal) {
        const QImage f = m_pipeline.lastFrame();
        setStatus(QStringLiteral("En directo: %1×%2 – escuchando en %3").arg(f.width()).arg(f.height()).arg(m_listenText));
    } else {
        m_video->setPlaceholder(QStringLiteral("Sin señal – esperando vídeo en %1…").arg(m_listenText));
        setStatus(QStringLiteral("Sin señal (no llegan fotogramas) – escuchando en %1").arg(m_listenText));
    }
}

} // namespace lanwindow
