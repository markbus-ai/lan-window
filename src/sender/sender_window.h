#pragma once

#include "common/config.h"
#include "common/port_field.h"
#include "common/sender_pipeline.h"
#include "common/theme.h"
#include "sender/portal_capture.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace lanwindow {

class SenderWindow : public QWidget {
    Q_OBJECT
public:
    explicit SenderWindow(QWidget *parent = nullptr);
    ~SenderWindow() override;

    void setDestination(const QString &host, int port);
    void setQualityIndex(int index);
    void setTestPattern(bool enabled);
    bool startSending();
    void stopSending();

    SenderPipeline *pipeline() { return &m_pipeline; }
    QString statusText() const;

private:
    bool collectConfig(SenderConfig *cfg, QString *error) const;
    bool startPipeline(SenderConfig cfg);
    void setStatus(const QString &text, SlabTone tone, const QString &word);
    void updateButtons();

    SenderPipeline m_pipeline;
    PortalCapture m_portal;
    QLineEdit *m_host = nullptr;
    PortField *m_port = nullptr;
    QComboBox *m_quality = nullptr;
    QCheckBox *m_testPattern = nullptr;
    QPushButton *m_startStop = nullptr;
    QLabel *m_slab = nullptr;
    QLabel *m_status = nullptr;
    bool m_waitingPortal = false;
    bool m_canSend = true;
    bool m_canCapture = true;
};

} // namespace lanwindow
