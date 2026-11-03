#pragma once

#include <QString>

class QApplication;
class QLabel;
class QWidget;

namespace lanwindow {

// Significado de la franja de estado; cada valor tiene su color en la hoja de estilo.
enum class SlabTone { Idle, Waiting, Live, Error };

// Carga las tipografías incluidas y aplica el estilo a toda la aplicación.
void applyTheme(QApplication &app);

// Actualiza la franja de estado (palabra y color) y la línea de detalle que la acompaña.
void setStateSlab(QLabel *slab, QLabel *detail, const QString &word, const QString &text, SlabTone tone);

// Marca un campo con un valor rechazado.
void setFieldInvalid(QWidget *field, bool invalid);

} // namespace lanwindow
