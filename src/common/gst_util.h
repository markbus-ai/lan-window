#pragma once

#include <QString>
#include <QStringList>
#include <vector>

#include <gst/gst.h>

namespace lanwindow {

// Inicializa GStreamer una sola vez (seguro llamar varias veces).
void ensureGstInit();

// Elementos necesarios para cada rol. El emisor necesita además un codificador H.264.
QStringList requiredReceiverElements();
QStringList requiredSenderElements(bool withPipeWire);

// Nombres de fábricas ausentes del registro.
QStringList missingElements(const QStringList &factories);

// Codificador H.264 preferido disponible ("x264enc" u "openh264enc"), vacío si ninguno.
QString pickH264Encoder();

// Codifica un fotograma real para descartar codificadores "stub" (p. ej. openh264 sin binario).
bool probeEncoder(const QString &factory, QString *error);

// Crea un elemento; si falla añade un mensaje legible a *error.
GstElement *makeElement(const char *factory, const char *name, QString *error);

struct BusEvent {
    enum Type { Error, Warning, Eos } type;
    QString text;
};

// Extrae sin bloquear los mensajes de error/aviso/EOS pendientes del bus.
std::vector<BusEvent> drainBus(GstElement *pipeline);

// Detiene y libera un pipeline (pone NULL y hace unref). Deja *pipeline a nullptr.
void destroyPipeline(GstElement **pipeline);

} // namespace lanwindow
