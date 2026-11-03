#include "receiver/receiver_window.h"

#include "common/gst_util.h"
#include "receiver/video_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace lanwindow {

namespace {

constexpr int kControlBarHeight = 36;

} // namespace

ReceiverWindow::ReceiverWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("LAN Window – Receptor"));
    resize(1280, 760);

    m_controls = new QWidget(this);
    m_controls->setObjectName(QStringLiteral("receiverControls"));
    m_controls->setAttribute(Qt::WA_StyledBackground);
    auto *row = new QHBoxLayout(m_controls);
    row->setContentsMargins(12, 8, 12, 8);
    row->setSpacing(10);
    m_address = new QLineEdit(QStringLiteral("0.0.0.0"), m_controls);
    m_address->setToolTip(QStringLiteral("IPv4 local donde escuchar. 0.0.0.0 = todas las interfaces."));
    m_address->setMaximumWidth(140);
    m_port = new PortField(m_controls);
    m_port->setMaximumWidth(90);
    m_port->setValue(kDefaultPort);
    m_startStop = new QPushButton(m_controls);
    m_startStop->setObjectName(QStringLiteral("primary"));
    m_startStop->setFixedSize(110, kControlBarHeight);
    auto *hide = new QPushButton(QStringLiteral("Ocultar controles (F11)"), m_controls);
    m_slab = new QLabel(m_controls);
    m_slab->setObjectName(QStringLiteral("stateSlab"));
    m_slab->setAlignment(Qt::AlignCenter);
    m_slab->setFixedSize(110, kControlBarHeight);
    m_status = new ElidedLabel(m_controls);
    const auto fieldLabel = [this](const QString &text) {
        auto *label = new QLabel(text, m_controls);
        label->setObjectName(QStringLiteral("fieldLabel"));
        return label;
    };
    row->addWidget(m_slab);
    row->addSpacing(8);
    row->addWidget(fieldLabel(QStringLiteral("Escuchar en")));
    row->addWidget(m_address);
    row->addWidget(fieldLabel(QStringLiteral("Puerto UDP")));
    row->addWidget(m_port);
    row->addWidget(m_startStop);
    row->addSpacing(8);
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
    const auto startOnReturn = [this] {
        if (!m_pipeline.isRunning())
            startReceiving();
    };
    connect(m_address, &QLineEdit::returnPressed, this, startOnReturn);
    connect(m_port, &QLineEdit::returnPressed, this, startOnReturn);
    connect(m_address, &QLineEdit::textEdited, this, [this] { setFieldInvalid(m_address, false); });
    connect(hide, &QPushButton::clicked, this, [this] { setControlsVisible(false); });
    auto *toggle = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(toggle, &QShortcut::activated, this, [this] { setControlsVisible(!m_controls->isVisible()); });
    auto *show = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(show, &QShortcut::activated, this, [this] { setControlsVisible(true); });

    connect(&m_pipeline, &ReceiverPipeline::frameReady, m_video, &VideoWidget::setFrame);
    connect(&m_pipeline, &ReceiverPipeline::signalChanged, this, &ReceiverWindow::onSignalChanged);
    connect(&m_pipeline, &ReceiverPipeline::failed, this, [this](const QString &msg) {
        m_video->setPlaceholder(QStringLiteral("Error"), msg);
        setStatus(QStringLiteral("Error: %1").arg(msg), SlabTone::Error, QStringLiteral("Error"));
        updateButtons();
    });
    connect(&m_pipeline, &ReceiverPipeline::warning, this,
            [](const QString &msg) { qWarning("Aviso GStreamer: %s", qPrintable(msg)); });

    const QStringList missing = missingElements(requiredReceiverElements());
    m_diagnosticsOk = missing.isEmpty();
    if (m_diagnosticsOk) {
        m_video->setPlaceholder(QStringLiteral("Detenido"));
        setStatus(QStringLiteral("Detenido"), SlabTone::Idle, QStringLiteral("Detenido"));
    } else {
        const QString msg = QStringLiteral("Faltan plugins GStreamer: %1").arg(missing.join(", "));
        m_video->setPlaceholder(QStringLiteral("Error"), msg);
        setStatus(msg, SlabTone::Error, QStringLiteral("Error"));
    }
    updateButtons();
    m_startStop->setFocus();
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
    setFieldInvalid(m_address, !isValidIPv4(cfg.bindAddress));
    if (!m_pipeline.start(cfg, &error)) {
        setStatus(QStringLiteral("Error: %1").arg(error), SlabTone::Error, QStringLiteral("Error"));
        m_video->setPlaceholder(QStringLiteral("Error"), error);
        if (!isValidIPv4(cfg.bindAddress))
            m_address->setFocus();
        updateButtons();
        return false;
    }
    m_listenText = QStringLiteral("%1:%2").arg(cfg.bindAddress).arg(m_pipeline.boundPort());
    m_video->setPlaceholder(QStringLiteral("Escuchando"));
    setStatus(QStringLiteral("Escuchando en %1 – esperando vídeo").arg(m_listenText), SlabTone::Waiting, QStringLiteral("Escuchando"));
    updateButtons();
    return true;
}

void ReceiverWindow::stopReceiving()
{
    m_pipeline.stop();
    m_video->setPlaceholder(QStringLiteral("Detenido"));
    setStatus(QStringLiteral("Detenido"), SlabTone::Idle, QStringLiteral("Detenido"));
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

void ReceiverWindow::setStatus(const QString &text, SlabTone tone, const QString &word)
{
    setStateSlab(m_slab, m_status, word, text, tone);
    m_status->setToolTip(text);
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
        setStatus(QStringLiteral("En directo: %1×%2 – escuchando en %3").arg(f.width()).arg(f.height()).arg(m_listenText),
                  SlabTone::Live, QStringLiteral("En directo"));
    } else {
        m_video->setPlaceholder(QStringLiteral("Sin señal"));
        setStatus(QStringLiteral("Sin señal (no llegan fotogramas) – escuchando en %1").arg(m_listenText),
                  SlabTone::Waiting, QStringLiteral("Sin señal"));
    }
}

} // namespace lanwindow
