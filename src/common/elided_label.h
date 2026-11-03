#pragma once

#include <QLabel>
#include <QPainter>
#include <QStyle>

namespace lanwindow {

// Etiqueta de una línea que recorta el texto con "…" en vez de ensanchar la ventana.
class ElidedLabel : public QLabel {
public:
    using QLabel::QLabel;

    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const QString shown = fontMetrics().elidedText(text(), Qt::ElideRight, width());
        style()->drawItemText(&painter, rect(), Qt::AlignLeft | Qt::AlignVCenter, palette(), isEnabled(), shown,
                              foregroundRole());
    }
};

} // namespace lanwindow
