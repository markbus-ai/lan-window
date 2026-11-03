#pragma once

#include <QImage>
#include <QWidget>

namespace lanwindow {

// Dibuja el vídeo con bandas negras (relación de aspecto preservada) dentro de la propia ventana,
// sin superposiciones nativas, para que la captura de ventana de Discord lo vea.
class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

    void setFrame(const QImage &frame);
    void setPlaceholder(const QString &text, const QString &detail = QString());
    QString placeholder() const { return m_placeholder; } // borra la imagen y muestra un texto
    QImage frame() const { return m_frame; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_frame;
    QString m_placeholder;
    QString m_placeholderDetail;
};

} // namespace lanwindow
