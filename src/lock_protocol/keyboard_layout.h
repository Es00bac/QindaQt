// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QVariantList>
struct wl_registry;
struct wl_seat;
struct wl_keyboard;
struct wl_surface;
struct wl_array;
struct xkb_context;
struct xkb_keymap;
namespace QindaQt::LockProtocol {
// Owned native read-only observer on the greeter's existing privileged display.
// It never synthesizes seat events or accepts credentials. Name/key labels come
// from the compositor's actual bounded XKB keymap/current group, not QLocale.
class KeyboardLayout final : public QObject {
  Q_OBJECT
public:
  KeyboardLayout(wl_registry *registry, unsigned name, unsigned version);
  ~KeyboardLayout() override;
  QString name() const { return m_name; }
  QVariantList rows() const { return m_rows; }
Q_SIGNALS:
  void changed();
private:
  void rebuild();
  static void capabilities(void *, wl_seat *, unsigned);
  static void seatName(void *, wl_seat *, const char *);
  static void keymap(void *, wl_keyboard *, unsigned, int, unsigned);
  static void enter(void *, wl_keyboard *, unsigned, wl_surface *, wl_array *);
  static void leave(void *, wl_keyboard *, unsigned, wl_surface *);
  static void key(void *, wl_keyboard *, unsigned, unsigned, unsigned, unsigned);
  static void modifiers(void *, wl_keyboard *, unsigned, unsigned, unsigned, unsigned, unsigned);
  static void repeat(void *, wl_keyboard *, int, int);
  wl_seat *m_seat = nullptr;
  wl_keyboard *m_keyboard = nullptr;
  xkb_context *m_context = nullptr;
  xkb_keymap *m_keymap = nullptr;
  unsigned m_group = 0;
  QString m_name;
  QVariantList m_rows;
};
}
