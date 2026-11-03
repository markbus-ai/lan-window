#pragma once

#include "common/config.h"

#include <QIntValidator>
#include <QLineEdit>

namespace lanwindow {

// Campo de puerto UDP: vacío o fuera de rango equivale al puerto predeterminado.
class PortField : public QLineEdit {
public:
    explicit PortField(QWidget *parent = nullptr) : QLineEdit(parent)
    {
        setValidator(new QIntValidator(1, 65535, this));
        setPlaceholderText(QString::number(kDefaultPort));
    }

    int value() const
    {
        const int port = text().toInt();
        return port >= 1 && port <= 65535 ? port : kDefaultPort;
    }

    void setValue(int port) { setText(QString::number(port >= 1 && port <= 65535 ? port : kDefaultPort)); }
};

} // namespace lanwindow
