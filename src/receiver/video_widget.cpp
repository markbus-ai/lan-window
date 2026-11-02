#include "receiver/video_widget.h"

#include <QPainter>

namespace lanwindow {

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoWidget::setFrame(const QImage &frame)
{
    m_frame = frame;
    update();
}

void VideoWidget::setPlaceholder(const QString &text)
{
    m_frame = QImage();
    m_placeholder = text;
    update();
}

void VideoWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_frame.isNull()) {
        p.setPen(QColor(200, 200, 200));
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.6);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, m_placeholder);
        return;
    }
    const QSize scaled = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
    if (scaled == m_frame.size()) {
        p.drawImage(target.topLeft(), m_frame);
    } else {
        // Escalado rápido: el suavizado a 1080p en cada fotograma es caro en CPU.
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(target, m_frame);
    }
}

} // namespace lanwindow
