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

void VideoWidget::setPlaceholder(const QString &text, const QString &detail)
{
    m_frame = QImage();
    m_placeholder = text;
    m_placeholderDetail = detail;
    update();
}

void VideoWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_frame.isNull()) {
        p.setPen(QColor(0x9a, 0xa1, 0xaa));
        QFont f(QStringLiteral("Barlow"));
        f.setPixelSize(24);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        if (m_placeholderDetail.isEmpty()) {
            p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, m_placeholder);
            return;
        }
        // Con detalle (errores): la palabra encima del centro y el motivo completo debajo.
        const int middle = height() / 2;
        p.drawText(QRect(0, 0, width(), middle), Qt::AlignHCenter | Qt::AlignBottom, m_placeholder);
        QFont detailFont(QStringLiteral("Barlow"));
        detailFont.setPixelSize(16);
        p.setFont(detailFont);
        p.drawText(rect().adjusted(32, middle + 16, -32, -16), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                   m_placeholderDetail);
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
