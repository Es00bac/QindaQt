// SPDX-License-Identifier: GPL-3.0-or-later
#include "protocol_client.h"
#include "ext-session-lock-v1-client-protocol.h"
#include <wayland-client.h>
namespace QindaQt::LockProtocol {
ProtocolClient::ProtocolClient(wl_display *display, wl_registry *registry, unsigned managerName)
    : m_display(display) {
  if (!display || !registry || !managerName) return;
  m_manager = static_cast<ext_session_lock_manager_v1 *>(
      wl_registry_bind(registry, managerName, &ext_session_lock_manager_v1_interface, 1));
  if (!m_manager) return;
  m_lock = ext_session_lock_manager_v1_lock(m_manager);
  if (!m_lock) return;
  static const ext_session_lock_v1_listener listener{onLocked, onFinished};
  ext_session_lock_v1_add_listener(m_lock, &listener, this);
  wl_display_flush(m_display);
}
ProtocolClient::~ProtocolClient() {
  if (m_sync) wl_callback_destroy(m_sync);
  // AGENT-GUARD: local proxy destruction leaves the server lock intact when a
  // client exits/crashes. Never send destroy on a locked resource or pretend
  // that a QObject destructor establishes authentication approval.
  if (m_lock) wl_proxy_destroy(reinterpret_cast<wl_proxy *>(m_lock));
  if (m_manager) ext_session_lock_manager_v1_destroy(m_manager);
}
ext_session_lock_surface_v1 *ProtocolClient::createSurface(wl_surface *surface, wl_output *output) {
  if (!available() || !surface || !output || m_unlocking) return nullptr;
  return ext_session_lock_v1_get_lock_surface(m_lock, surface, output);
}
void ProtocolClient::onLocked(void *data, ext_session_lock_v1 *) {
  auto *self = static_cast<ProtocolClient *>(data);
  if (self->m_locked || self->m_finished || self->m_unlocking) return;
  self->m_locked = true; Q_EMIT self->locked();
}
void ProtocolClient::onFinished(void *data, ext_session_lock_v1 *) {
  auto *self = static_cast<ProtocolClient *>(data);
  self->m_finished = true; Q_EMIT self->rejected();
}
bool ProtocolClient::unlockAuthenticated() {
  if (!m_lock || !m_locked || m_finished || m_unlocking) return false;
  m_unlocking = true;
  ext_session_lock_v1_unlock_and_destroy(m_lock); m_lock = nullptr;
  // Wait for server processing before closing the inherited connection. A
  // failure/EOF merely preserves the server's fail-closed lock enforcement.
  m_sync = wl_display_sync(m_display);
  if (!m_sync) return false;
  static const wl_callback_listener listener{onSync};
  wl_callback_add_listener(m_sync, &listener, this);
  return wl_display_flush(m_display) >= 0;
}
void ProtocolClient::onSync(void *data, wl_callback *callback, unsigned) {
  auto *self = static_cast<ProtocolClient *>(data);
  wl_callback_destroy(callback); self->m_sync = nullptr; self->m_locked = false;
  Q_EMIT self->unlockProcessed();
}
} // namespace QindaQt::LockProtocol
