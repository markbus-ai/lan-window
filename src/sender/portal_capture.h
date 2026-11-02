#pragma once

#include <QObject>
#include <QTimer>

// Declaraciones adelantadas: las cabeceras de GIO usan el identificador "signals", que choca con Qt.
struct _GAsyncResult;
struct _GCancellable;
struct _GError;
struct _GObject;
struct _XdpPortal;
struct _XdpSession;

namespace lanwindow {

// Sesión ScreenCast del portal XDG (vía libportal): CreateSession/SelectSources/Start y
// OpenPipeWireRemote. Conserva la sesión y el descriptor PipeWire mientras dura la captura.
class PortalCapture : public QObject {
    Q_OBJECT
public:
    explicit PortalCapture(QObject *parent = nullptr);
    ~PortalCapture() override;

    // Abre el selector del sistema. Resultado asíncrono: ready(), cancelled() o failed().
    void start();
    // Cierra la sesión y el descriptor; los callbacks pendientes se ignoran. Detén antes el pipeline.
    void stop();
    bool isBusy() const { return m_session != nullptr || m_cancellable != nullptr; }

signals:
    // fd pertenece a PortalCapture y sigue abierto hasta stop(); nodeId es el id de nodo PipeWire.
    void ready(int fd, quint32 nodeId);
    void cancelled();
    void failed(const QString &message);
    void closed(); // la sesión se cerró desde fuera (compositor/usuario)

private:
    static void onSessionCreated(_GObject *source, _GAsyncResult *res, void *data);
    static void onSessionStarted(_GObject *source, _GAsyncResult *res, void *data);
    static void onSessionClosed(_XdpSession *session, void *data);
    void handleError(_GError *error);

    _XdpPortal *m_portal = nullptr;
    _XdpSession *m_session = nullptr;
    _GCancellable *m_cancellable = nullptr;
    int m_fd = -1;
    quint64 m_generation = 0;
    QTimer m_glibPump; // solo si el despachador de eventos de Qt no integra GLib
};

} // namespace lanwindow
