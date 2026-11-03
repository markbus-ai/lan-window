#include "sender/sender_window.h"

#include "common/gst_util.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace lanwindow {

SenderWindow::SenderWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(QStringLiteral("LAN Window – Emisor"));
    setObjectName(QStringLiteral("senderRoot"));
    setAttribute(Qt::WA_StyledBackground);

    QSettings settings;
    m_host = new QLineEdit(settings.value("host").toString(), this);
    m_host->setPlaceholderText(QStringLiteral("IPv4 del PC con Windows, p. ej. 192.168.1.50"));
    m_host->setObjectName(QStringLiteral("hostField"));
    m_port = new PortField(this);
    m_port->setValue(settings.value("port", kDefaultPort).toInt());
    m_quality = new QComboBox(this);
    int count = 0;
    const QualityPreset *presets = qualityPresets(&count);
    for (int i = 0; i < count; ++i)
        m_quality->addItem(QString::fromUtf8(presets[i].label));
    m_quality->setCurrentIndex(qBound(0, settings.value("quality", 0).toInt(), count - 1));
    m_testPattern = new QCheckBox(QStringLiteral("Enviar patrón de prueba (sin capturar la pantalla)"), this);
    m_startStop = new QPushButton(this);
    m_startStop->setObjectName(QStringLiteral("primary"));
    m_slab = new QLabel(this);
    m_slab->setObjectName(QStringLiteral("stateSlab"));
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setMinimumWidth(420);
    m_status->setMinimumHeight(40); // dos líneas: el formulario no salta entre estados
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    const auto fieldLabel = [this](const QString &text) {
        auto *label = new QLabel(text, this);
        label->setObjectName(QStringLiteral("fieldLabel"));
        return label;
    };
    auto *form = new QGridLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setColumnStretch(1, 1);
    form->addWidget(fieldLabel(QStringLiteral("Destino (IPv4)")), 0, 0, Qt::AlignVCenter);
    form->addWidget(m_host, 0, 1);
    form->addWidget(fieldLabel(QStringLiteral("Puerto UDP")), 1, 0, Qt::AlignVCenter);
    form->addWidget(m_port, 1, 1);
    form->addWidget(fieldLabel(QStringLiteral("Calidad")), 2, 0, Qt::AlignVCenter);
    form->addWidget(m_quality, 2, 1);
    form->addWidget(m_testPattern, 3, 1);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    layout->addWidget(m_slab);
    layout->addSpacing(8);
    layout->addWidget(m_status);
    layout->addSpacing(12);
    layout->addLayout(form);
    layout->addSpacing(20);
    layout->addWidget(m_startStop);

    connect(m_startStop, &QPushButton::clicked, this, [this] {
        if (m_pipeline.isRunning() || m_waitingPortal)
            stopSending();
        else
            startSending();
    });
    const auto startOnReturn = [this] {
        if (!m_pipeline.isRunning() && !m_waitingPortal)
            startSending();
    };
    connect(m_host, &QLineEdit::returnPressed, this, startOnReturn);
    connect(m_port, &QLineEdit::returnPressed, this, startOnReturn);
    connect(m_host, &QLineEdit::textEdited, this, [this] { setFieldInvalid(m_host, false); });

    connect(&m_pipeline, &SenderPipeline::failed, this, [this](const QString &msg) {
        m_portal.stop();
        setStatus(QStringLiteral("Error: %1").arg(msg), SlabTone::Error, QStringLiteral("Error"));
        updateButtons();
    });
    connect(&m_pipeline, &SenderPipeline::finished, this, [this] {
        m_portal.stop();
        setStatus(QStringLiteral("La captura terminó (flujo de PipeWire cerrado)."), SlabTone::Error, QStringLiteral("Cortado"));
        updateButtons();
    });
    connect(&m_pipeline, &SenderPipeline::warning, this,
            [](const QString &msg) { qWarning("Aviso GStreamer: %s", qPrintable(msg)); });

    connect(&m_portal, &PortalCapture::ready, this, [this](int fd, quint32 nodeId) {
        m_waitingPortal = false;
        SenderConfig cfg;
        QString error;
        if (!collectConfig(&cfg, &error)) {
            m_portal.stop();
            setStatus(QStringLiteral("Error: %1").arg(error), SlabTone::Error, QStringLiteral("Error"));
            updateButtons();
            return;
        }
        cfg.source = SourceKind::PipeWire;
        cfg.pipewireFd = fd;
        cfg.pipewireNodeId = nodeId;
        if (!startPipeline(cfg))
            m_portal.stop();
    });
    connect(&m_portal, &PortalCapture::cancelled, this, [this] {
        m_waitingPortal = false;
        setStatus(QStringLiteral("Selección de pantalla cancelada."), SlabTone::Idle, QStringLiteral("Cancelado"));
        updateButtons();
    });
    connect(&m_portal, &PortalCapture::failed, this, [this](const QString &msg) {
        m_waitingPortal = false;
        m_pipeline.stop();
        setStatus(QStringLiteral("Error: %1").arg(msg), SlabTone::Error, QStringLiteral("Error"));
        updateButtons();
    });
    connect(&m_portal, &PortalCapture::closed, this, [this] {
        m_waitingPortal = false;
        m_pipeline.stop();
        setStatus(QStringLiteral("La sesión de captura se cerró desde el sistema."), SlabTone::Error, QStringLiteral("Cortado"));
        updateButtons();
    });

    // Diagnóstico al arrancar: plugins y un fotograma real con el codificador.
    QStringList problems;
    const QStringList missing = missingElements(requiredSenderElements(false));
    if (!missing.isEmpty()) {
        problems << QStringLiteral("Faltan plugins GStreamer: %1.").arg(missing.join(", "));
        m_canSend = false;
    }
    const QString encoder = pickH264Encoder();
    QString encError;
    if (encoder.isEmpty()) {
        problems << QStringLiteral("No hay codificador H.264 (instala x264 u openh264 para GStreamer).");
        m_canSend = false;
    } else if (!probeEncoder(encoder, &encError)) {
        problems << encError;
        m_canSend = false;
    }
    if (!missingElements({"pipewiresrc"}).isEmpty()) {
        problems << QStringLiteral("Falta pipewiresrc: solo está disponible el patrón de prueba.");
        m_canCapture = false;
        m_testPattern->setChecked(true);
    }
    if (problems.isEmpty())
        setStatus(QStringLiteral("Listo (codificador: %1).").arg(encoder), SlabTone::Idle, QStringLiteral("Listo"));
    else
        setStatus(problems.join(QLatin1Char('\n')), SlabTone::Error, QStringLiteral("Problema"));
    updateButtons();
}

SenderWindow::~SenderWindow()
{
    m_pipeline.stop(); // el pipeline debe soltar el fd antes de cerrar la sesión
    m_portal.stop();
}

void SenderWindow::setDestination(const QString &host, int port)
{
    m_host->setText(host);
    m_port->setValue(port);
}

void SenderWindow::setQualityIndex(int index)
{
    m_quality->setCurrentIndex(index);
}

void SenderWindow::setTestPattern(bool enabled)
{
    m_testPattern->setChecked(enabled || !m_canCapture);
}

QString SenderWindow::statusText() const
{
    return m_status->text();
}

bool SenderWindow::collectConfig(SenderConfig *cfg, QString *error) const
{
    int count = 0;
    const QualityPreset &q = qualityPresets(&count)[qBound(0, m_quality->currentIndex(), count - 1)];
    cfg->host = m_host->text().trimmed();
    cfg->port = m_port->value();
    cfg->width = q.width;
    cfg->height = q.height;
    cfg->fps = q.fps;
    cfg->bitrateKbps = q.bitrateKbps;
    cfg->source = SourceKind::TestPattern;
    *error = validateSenderConfig(*cfg);
    return error->isEmpty();
}

bool SenderWindow::startSending()
{
    if (!m_canSend)
        return false;
    SenderConfig cfg;
    QString error;
    if (!collectConfig(&cfg, &error)) {
        setStatus(QStringLiteral("Error: %1").arg(error), SlabTone::Error, QStringLiteral("Error"));
        setFieldInvalid(m_host, !isValidIPv4(cfg.host));
        m_host->setFocus();
        return false;
    }
    setFieldInvalid(m_host, false);
    QSettings settings;
    settings.setValue("host", cfg.host);
    settings.setValue("port", cfg.port);
    settings.setValue("quality", m_quality->currentIndex());

    if (m_testPattern->isChecked())
        return startPipeline(cfg);

    m_waitingPortal = true;
    setStatus(QStringLiteral("Elige la pantalla en el diálogo del sistema…"), SlabTone::Waiting, QStringLiteral("Esperando"));
    updateButtons();
    m_portal.start();
    return true;
}

bool SenderWindow::startPipeline(SenderConfig cfg)
{
    QString error;
    if (!m_pipeline.start(cfg, &error)) {
        setStatus(QStringLiteral("Error: %1").arg(error), SlabTone::Error, QStringLiteral("Error"));
        updateButtons();
        return false;
    }
    const QString what = cfg.source == SourceKind::PipeWire ? QStringLiteral("pantalla")
                                                            : QStringLiteral("patrón de prueba");
    setStatus(QStringLiteral("Enviando %1 a %2:%3 – %4×%5 a %6 fps (%7).")
                  .arg(what, cfg.host)
                  .arg(cfg.port)
                  .arg(cfg.width)
                  .arg(cfg.height)
                  .arg(cfg.fps)
                  .arg(m_pipeline.encoderName()),
              SlabTone::Live, QStringLiteral("Enviando"));
    updateButtons();
    return true;
}

void SenderWindow::stopSending()
{
    m_waitingPortal = false;
    m_pipeline.stop();
    m_portal.stop();
    setStatus(QStringLiteral("Detenido."), SlabTone::Idle, QStringLiteral("Detenido"));
    updateButtons();
}

void SenderWindow::setStatus(const QString &text, SlabTone tone, const QString &word)
{
    setStateSlab(m_slab, m_status, word, text, tone);
}

void SenderWindow::updateButtons()
{
    const bool busy = m_pipeline.isRunning() || m_waitingPortal;
    m_startStop->setText(busy ? QStringLiteral("Detener") : QStringLiteral("Iniciar"));
    m_startStop->setEnabled(m_canSend);
    m_host->setEnabled(!busy);
    m_port->setEnabled(!busy);
    m_quality->setEnabled(!busy);
    m_testPattern->setEnabled(!busy && m_canCapture);
}

} // namespace lanwindow
