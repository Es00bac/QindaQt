// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard_layout.h"
#include <QVariantMap>
#include <array>
#include <algorithm>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
namespace QindaQt::LockProtocol {
KeyboardLayout::KeyboardLayout(wl_registry *registry, unsigned name, unsigned version) {
  m_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  m_seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7U)));
  static const wl_seat_listener listener{capabilities, seatName};
  wl_seat_add_listener(m_seat, &listener, this);
}
KeyboardLayout::~KeyboardLayout() {
  if (m_keyboard) { if (wl_keyboard_get_version(m_keyboard) >= 3) wl_keyboard_release(m_keyboard); else wl_keyboard_destroy(m_keyboard); }
  if (m_seat) { if (wl_seat_get_version(m_seat) >= 5) wl_seat_release(m_seat); else wl_seat_destroy(m_seat); }
  if (m_keymap) xkb_keymap_unref(m_keymap);
  if (m_context) xkb_context_unref(m_context);
}
void KeyboardLayout::capabilities(void *data, wl_seat *seat, unsigned caps) {
  auto *self = static_cast<KeyboardLayout *>(data);
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !self->m_keyboard) {
    self->m_keyboard = wl_seat_get_keyboard(seat);
    static const wl_keyboard_listener listener{keymap, enter, leave, key, modifiers, repeat};
    wl_keyboard_add_listener(self->m_keyboard, &listener, self);
  }
}
void KeyboardLayout::seatName(void *, wl_seat *, const char *) {}
void KeyboardLayout::enter(void *, wl_keyboard *, unsigned, wl_surface *, wl_array *) {}
void KeyboardLayout::leave(void *, wl_keyboard *, unsigned, wl_surface *) {}
void KeyboardLayout::key(void *, wl_keyboard *, unsigned, unsigned, unsigned, unsigned) {}
void KeyboardLayout::repeat(void *, wl_keyboard *, int, int) {}
void KeyboardLayout::keymap(void *data, wl_keyboard *, unsigned format, int fd, unsigned size) {
  auto *self = static_cast<KeyboardLayout *>(data);
  if (!self->m_context || format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size < 2 || size > 1024 * 1024) { close(fd); return; }
  struct stat info{};
  if (fstat(fd, &info) || info.st_size < size) { close(fd); return; }
  void *bytes = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0); close(fd);
  if (bytes == MAP_FAILED) return;
  auto *map = xkb_keymap_new_from_buffer(self->m_context, static_cast<const char *>(bytes),
                                       size - 1, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(bytes, size);
  if (!map) return;
  if (self->m_keymap) xkb_keymap_unref(self->m_keymap);
  self->m_keymap = map; self->m_group = 0; self->rebuild();
}
void KeyboardLayout::modifiers(void *data, wl_keyboard *, unsigned, unsigned, unsigned, unsigned, unsigned group) {
  auto *self = static_cast<KeyboardLayout *>(data);
  if (self->m_group != group) { self->m_group = group; self->rebuild(); }
}
void KeyboardLayout::rebuild() {
  if (!m_keymap || m_group >= xkb_keymap_num_layouts(m_keymap)) return;
  const char *name = xkb_keymap_layout_get_name(m_keymap, m_group);
  m_name = name ? QString::fromUtf8(name) : QString();
  m_rows.clear();
  const std::array<const char *, 4> prefixes{"AE", "AD", "AC", "AB"};
  const std::array<unsigned, 4> counts{12, 12, 11, 10};
  for (std::size_t row = 0; row < prefixes.size(); ++row) {
    QVariantList keys;
    for (unsigned i = 1; i <= counts[row]; ++i) {
      const auto keyName = QString::fromLatin1(prefixes[row]) + QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0'));
      const auto code = xkb_keymap_key_by_name(m_keymap, keyName.toLatin1().constData());
      if (code == XKB_KEYCODE_INVALID) continue;
      auto label = [&](unsigned level) {
        const xkb_keysym_t *symbols = nullptr;
        if (xkb_keymap_key_get_syms_by_level(m_keymap, code, m_group, level, &symbols) != 1) return QString();
        char text[8]{}; const int length = xkb_keysym_to_utf8(symbols[0], text, sizeof(text));
        return length > 1 && length <= int(sizeof(text)) ? QString::fromUtf8(text) : QString();
      };
      const auto normal = label(0); const auto shifted = label(1);
      if (!normal.isEmpty() && normal[0].isPrint()) keys.append(QVariantMap{{QStringLiteral("normal"), normal}, {QStringLiteral("shifted"), shifted.isEmpty() ? normal : shifted}});
    }
    m_rows.append(QVariant(keys));
  }
  Q_EMIT changed();
}
}
