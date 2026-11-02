// libportal/GIO antes que Qt: GIO usa "signals" como identificador.
#include <libportal/portal.h>
#include <unistd.h>

#include "sender/portal_capture.h"

#include <QAbstractEventDispatcher>
#include <QPointer>

namespace lanwindow {

namespace {

// Datos de cada llamada asíncrona: el QPointer y la generación permiten descartar resultados
// que llegan tras stop() o tras destruir el objeto.
struct CallData {
    QPointer<PortalCapture> owner;
    quint64 generation;
};

} // namespace

PortalCapture::PortalCapture(QObject *parent) : QObject(parent)
{
    m_glibPump.setInterval(10);
    connect(&m_glibPump, &QTimer::timeout, this, [] {
        while (g_main_context_iteration(nullptr, FALSE)) {
        }
    });
}

PortalCapture::~PortalCapture()
{
    stop();
    if (m_portal)
        g_object_unref(m_portal);
}

void PortalCapture::start()
{
    stop();
    if (!m_portal) {
        GError *error = nullptr;
        m_portal = xdp_portal_initable_new(&error);
        if (!m_portal) {
            const QString msg = QStringLiteral("No se pudo conectar con el portal XDG: %1")
                                    .arg(QString::fromUtf8(error ? error->message : "desconocido"));
            g_clear_error(&error);
            emit failed(msg);
            return;
        }
    }
    // En Linux Qt suele usar el bucle GLib; si no, bombeamos el contexto por defecto sin bloquear.
    QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance();
    if (!dispatcher || !dispatcher->inherits("QEventDispatcherGlib"))
        m_glibPump.start();

    m_cancellable = g_cancellable_new();
    auto *data = new CallData{this, m_generation};
    xdp_portal_create_screencast_session(m_portal, XDP_OUTPUT_MONITOR, XDP_SCREENCAST_FLAG_NONE,
                                         XDP_CURSOR_MODE_EMBEDDED, XDP_PERSIST_MODE_NONE, nullptr,
                                         m_cancellable, onSessionCreated, data);
}

void PortalCapture::stop()
{
    ++m_generation; // invalida callbacks pendientes
    if (m_cancellable) {
        g_cancellable_cancel(m_cancellable);
        g_clear_object(&m_cancellable);
    }
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
    if (m_session) {
        g_signal_handlers_disconnect_by_data(m_session, this);
        xdp_session_close(m_session);
        g_clear_object(&m_session);
    }
    m_glibPump.stop();
}

void PortalCapture::onSessionCreated(GObject *source, GAsyncResult *res, void *data)
{
    auto *call = static_cast<CallData *>(data);
    GError *error = nullptr;
    XdpSession *session = xdp_portal_create_screencast_session_finish(XDP_PORTAL(source), res, &error);
    PortalCapture *self = call->owner.data();
    const bool stale = !self || self->m_generation != call->generation;
    if (stale) {
        if (session) {
            xdp_session_close(session);
            g_object_unref(session);
        }
        g_clear_error(&error);
        delete call;
        return;
    }
    if (!session) {
        delete call;
        self->handleError(error);
        g_clear_error(&error);
        return;
    }
    self->m_session = session; // referencia completa (transfer full)
    g_signal_connect(session, "closed", G_CALLBACK(onSessionClosed), self);
    // Sin ventana padre: no hay integración Qt<->XdpParent; el diálogo aparece igualmente.
    xdp_session_start(session, nullptr, self->m_cancellable, onSessionStarted, call);
}

void PortalCapture::onSessionStarted(GObject *source, GAsyncResult *res, void *data)
{
    auto *call = static_cast<CallData *>(data);
    GError *error = nullptr;
    const gboolean ok = xdp_session_start_finish(XDP_SESSION(source), res, &error);
    PortalCapture *self = call->owner.data();
    const bool stale = !self || self->m_generation != call->generation;
    delete call;
    if (stale) {
        g_clear_error(&error); // la sesión ya se cerró en stop()
        return;
    }
    if (!ok) {
        self->handleError(error);
        g_clear_error(&error);
        return;
    }

    guint32 nodeId = 0;
    bool haveNode = false;
    if (GVariant *streams = xdp_session_get_streams(self->m_session)) { // a(ua{sv}), transfer full
        GVariantIter iter;
        g_variant_iter_init(&iter, streams);
        GVariant *props = nullptr;
        if (g_variant_iter_next(&iter, "(u@a{sv})", &nodeId, &props)) {
            haveNode = true;
            g_variant_unref(props);
        }
        g_variant_unref(streams);
    }
    if (!haveNode) {
        self->stop();
        emit self->failed(QStringLiteral("El portal no devolvió ningún flujo de pantalla."));
        return;
    }
    const int fd = xdp_session_open_pipewire_remote(self->m_session); // el descriptor es nuestro
    if (fd < 0) {
        self->stop();
        emit self->failed(QStringLiteral("No se pudo abrir el remoto de PipeWire del portal."));
        return;
    }
    self->m_fd = fd;
    g_clear_object(&self->m_cancellable);
    emit self->ready(fd, nodeId);
}

void PortalCapture::onSessionClosed(XdpSession *, void *data)
{
    auto *self = static_cast<PortalCapture *>(data);
    self->stop();
    emit self->closed();
}

void PortalCapture::handleError(GError *error)
{
    const bool userCancelled = error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    const QString msg = QString::fromUtf8(error ? error->message : "error desconocido del portal");
    stop();
    if (userCancelled)
        emit cancelled();
    else
        emit failed(QStringLiteral("Portal de captura: %1").arg(msg));
}

} // namespace lanwindow
