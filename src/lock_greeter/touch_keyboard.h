// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "osk_keyboard_model.h"
#include "protocol_client.h"
namespace QindaQt::LockGreeter {
// Reuses the public pure OSK geometry/state model, entirely inside the
// protected locker. Emits editing intent to its own credential field; it
// cannot send seat events or reach any ordinary application.
class TouchKeyboard final : public QObject, private Apps::Osk::KeyEmitter {
  Q_OBJECT
public:
  explicit TouchKeyboard(LockProtocol::ProtocolClient &protocol, QObject *parent = nullptr);
  Apps::Osk::OskKeyboardModel &model() { return m_model; }
Q_SIGNALS:
  void insertText(QString text);
  void backspace();
  void submit();
  void hide();
private:
  void rebuild();
  void commitText(const QString &text) override;
  void pressKeysym(quint32 keysym) override;
  LockProtocol::ProtocolClient &m_protocol;
  Apps::Osk::OskKeyboardModel m_model;
};
}
