// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>
struct wl_display;
struct wl_registry;
struct wl_surface;
struct wl_output;
struct wl_callback;
struct ext_session_lock_manager_v1;
struct ext_session_lock_v1;
struct ext_session_lock_surface_v1;
namespace QindaQt::LockProtocol {
// GUI-thread native boundary, owned by Qt's session-lock shell integration.
// Borrows the display until integration teardown; surfaces borrow this object.
// Never expose this object to QML. Only the native authentication controller may
// invoke unlockAuthenticated after consuming current epoch/request PAM approval.
// Role failure returns nullptr: the caller must exit, never fall back to a
// normal toplevel or layer-shell surface. Signals carry no credentials.
class KeyboardLayout;
class ProtocolClient final : public QObject {
  Q_OBJECT
public:
  ProtocolClient(wl_display *display, wl_registry *registry, unsigned managerName);
  ~ProtocolClient() override;
  ext_session_lock_surface_v1 *createSurface(wl_surface *surface, wl_output *output);
  bool isLocked() const { return m_locked; }
  bool available() const { return m_lock && !m_finished; }
  bool unlockAuthenticated();
  void observeKeyboard(wl_registry *registry, unsigned seatName, unsigned version);
  QString keyboardLayout() const;
  QVariantList keyboardRows() const;
Q_SIGNALS:
  void locked();
  void rejected();
  void unlockProcessed();
  void keyboardChanged();
private:
  static void onLocked(void *data, ext_session_lock_v1 *lock);
  static void onFinished(void *data, ext_session_lock_v1 *lock);
  static void onSync(void *data, wl_callback *callback, unsigned serial);
  std::unique_ptr<KeyboardLayout> m_keyboard;
  wl_display *m_display;
  ext_session_lock_manager_v1 *m_manager = nullptr;
  ext_session_lock_v1 *m_lock = nullptr;
  wl_callback *m_sync = nullptr;
  bool m_locked = false, m_finished = false, m_unlocking = false;
};
inline constexpr char nativeResource[] = "qindaqt-session-lock-client";
} // namespace QindaQt::LockProtocol
