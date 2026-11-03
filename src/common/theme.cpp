#include "common/theme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QLabel>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

// Q_INIT_RESOURCE no puede usarse dentro de un espacio de nombres.
static void initThemeResources()
{
    Q_INIT_RESOURCE(lanwindow);
}

namespace lanwindow {

namespace {

const char kStyleSheet[] = R"(
* {
    font-family: "Barlow";
    font-size: 15px;
    font-weight: 500;
    color: #e6e8eb;
}
QWidget#senderRoot, QWidget#receiverControls {
    background: #15171a;
}
QLabel#stateSlab {
    font-size: 22px;
    font-weight: 700;
    padding: 10px 14px;
    border-radius: 4px;
    qproperty-indent: 0;
}
QWidget#receiverControls QLabel#stateSlab {
    font-size: 15px;
    padding: 0;
}
QLabel#stateSlab[tone="idle"] { background: #2a2e34; color: #c3c8cf; }
QLabel#stateSlab[tone="waiting"] { background: #3a3212; color: #f2cf5b; }
QLabel#stateSlab[tone="live"] { background: #12351f; color: #57d98a; }
QLabel#stateSlab[tone="error"] { background: #3d1a16; color: #ff8a75; }
QLabel#fieldLabel {
    color: #9aa1aa;
    font-size: 14px;
}
QLineEdit, QComboBox {
    background: #1e2126;
    border: 1px solid #3a4048;
    border-radius: 4px;
    padding: 6px 8px;
    min-height: 22px;
    selection-background-color: #e6e8eb;
    selection-color: #15171a;
    placeholder-text-color: #9aa1aa;
}
QLineEdit:hover, QComboBox:hover {
    border-color: #6f7680;
}
QLineEdit:focus, QComboBox:focus {
    border-color: #e6e8eb;
}
QLineEdit[invalid="true"] {
    background: #3d1a16;
    border-color: #ff8a75;
}
QLineEdit:disabled, QComboBox:disabled {
    background: #15171a;
    color: #8a919b;
}
QComboBox::drop-down {
    border: none;
    width: 28px;
}
QComboBox::down-arrow {
    image: url(:/lanwindow/down-arrow.png);
    width: 11px;
    height: 8px;
}
QComboBox QAbstractItemView {
    background: #1e2126;
    border: 1px solid #3a4048;
    outline: none;
    selection-background-color: #e6e8eb;
    selection-color: #15171a;
}
QCheckBox {
    spacing: 10px;
}
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    background: #1e2126;
    border: 1px solid #6f7680;
    border-radius: 3px;
}
QCheckBox::indicator:focus {
    border-color: #e6e8eb;
}
QCheckBox::indicator:checked {
    background: #e6e8eb;
    border-color: #e6e8eb;
    image: url(:/lanwindow/check.png);
}
QCheckBox::indicator:disabled {
    background: #15171a;
    border-color: #3a4048;
}
QCheckBox::indicator:checked:disabled {
    background: #8a919b;
    border-color: #8a919b;
}
QCheckBox:disabled {
    color: #8a919b;
}
QPushButton {
    background: transparent;
    border: 1px solid #3a4048;
    border-radius: 4px;
    padding: 7px 14px;
}
QPushButton:hover {
    background: #2a2e34;
}
QPushButton:pressed {
    background: #3a4048;
}
QPushButton:focus {
    border-color: #e6e8eb;
}
QPushButton#primary {
    font-weight: 700;
    background: #e6e8eb;
    border-color: #e6e8eb;
    color: #15171a;
    padding: 9px 24px;
}
QPushButton#primary:hover {
    background: #ffffff;
    border-color: #ffffff;
}
QPushButton#primary:pressed {
    background: #c3c8cf;
    border-color: #c3c8cf;
}
QPushButton#primary:focus {
    border: 2px solid #57a6ff;
}
QPushButton#primary:disabled {
    background: #2a2e34;
    border-color: #2a2e34;
    color: #8a919b;
}
QToolTip {
    background: #2a2e34;
    color: #e6e8eb;
    border: 1px solid #3a4048;
    padding: 4px 6px;
}
)";

const char *toneName(SlabTone tone)
{
    switch (tone) {
    case SlabTone::Waiting:
        return "waiting";
    case SlabTone::Live:
        return "live";
    case SlabTone::Error:
        return "error";
    case SlabTone::Idle:
        break;
    }
    return "idle";
}

} // namespace

void applyTheme(QApplication &app)
{
    initThemeResources();
    QFontDatabase::addApplicationFont(QStringLiteral(":/lanwindow/fonts/Barlow-Medium.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/lanwindow/fonts/Barlow-Bold.ttf"));

    // Fusion respeta la hoja de estilo igual en Linux y Windows.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette = QApplication::palette();
    // Paleta fija: la ventana no depende del tema del sistema.
    const QColor text(0xe6, 0xe8, 0xeb);
    palette.setColor(QPalette::Window, QColor(0x15, 0x17, 0x1a));
    palette.setColor(QPalette::Base, QColor(0x1e, 0x21, 0x26));
    palette.setColor(QPalette::Button, QColor(0x2a, 0x2e, 0x34));
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::PlaceholderText, QColor(0x9a, 0xa1, 0xaa));
    QApplication::setPalette(palette);
    app.setStyleSheet(QString::fromLatin1(kStyleSheet));
}

void setFieldInvalid(QWidget *field, bool invalid)
{
    field->setProperty("invalid", invalid);
    field->style()->unpolish(field);
    field->style()->polish(field);
}

void setStateSlab(QLabel *slab, QLabel *detail, const QString &word, const QString &text, SlabTone tone)
{
    // El detalle se conserva como texto de estado, pero no se muestra si solo repite la franja.
    detail->setText(text);
    QSizePolicy policy = detail->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    detail->setSizePolicy(policy);
    QString plain = text;
    if (plain.endsWith(QLatin1Char('.')))
        plain.chop(1);
    detail->setVisible(plain.compare(word, Qt::CaseInsensitive) != 0);

    slab->setText(word);
    slab->setProperty("tone", QString::fromLatin1(toneName(tone)));
    slab->style()->unpolish(slab);
    slab->style()->polish(slab);
}

} // namespace lanwindow
