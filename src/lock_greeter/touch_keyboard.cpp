// SPDX-License-Identifier: GPL-3.0-or-later
#include "touch_keyboard.h"
#include "osk_layout_catalog.h"
#include <QVariantMap>
#include <xkbcommon/xkbcommon-keysyms.h>
namespace QindaQt::LockGreeter {
using namespace Apps::Osk;
TouchKeyboard::TouchKeyboard(LockProtocol::ProtocolClient &protocol, QObject *parent)
    : QObject(parent), m_protocol(protocol) {
  m_model.setEmitter(this);
  connect(&protocol, &LockProtocol::ProtocolClient::keyboardChanged, this, &TouchKeyboard::rebuild);
  connect(&m_model, &OskKeyboardModel::hideRequested, this, &TouchKeyboard::hide);
  rebuild();
}
void TouchKeyboard::rebuild() {
  // The compositor's current XKB group supplies actual printable labels.
  // The existing OSK core owns shift/page policy, geometry and symbols.
  auto document = OskLayoutCatalog().documentFor(QStringLiteral("us"));
  const auto rows = m_protocol.keyboardRows();
  if (!rows.isEmpty()) {
    document.name = m_protocol.keyboardLayout(); document.label = document.name; document.letters.clear();
    for (const auto &row : rows) {
      KeyRow keys;
      for (const auto &entry : row.toList()) {
        const auto value = entry.toMap();
        keys.append({KeyKind::Text, value.value(QStringLiteral("normal")).toString(),
                     value.value(QStringLiteral("shifted")).toString(), 1.0});
      }
      document.letters.append(keys);
    }
    document.letters[2].prepend({KeyKind::Shift, {}, {}, 1.5});
    document.letters[2].append({KeyKind::Backspace, {}, {}, 1.5});
    document.letters.append({{KeyKind::Symbols, {}, {}, 1.5}, {KeyKind::Space, {}, {}, 6.0},
                             {KeyKind::Enter, {}, {}, 2.0}, {KeyKind::Hide, {}, {}, 1.5}});
  }
  m_model.setDocument(document); m_model.resetTransientState();
}
void TouchKeyboard::commitText(const QString &text) { Q_EMIT insertText(text); }
void TouchKeyboard::pressKeysym(quint32 keysym) {
  if (keysym == XKB_KEY_BackSpace) Q_EMIT backspace();
  else if (keysym == XKB_KEY_Return) Q_EMIT submit();
}
}
