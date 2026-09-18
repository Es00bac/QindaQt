// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "osk_keyboard_model.h"
#include "qwayland-input-method-unstable-v1.h"

#include <QObject>
#include <QString>
#include <QtWaylandClient/QWaylandClientExtension>
#include <memory>

namespace QindaQt::Apps::Osk {

// The active `zwp_input_method_context_v1`: the compositor hands one over
// while a text field is focused. Text and keysyms travel through it.
class InputMethodContextV1 final : public QObject, public QtWayland::zwp_input_method_context_v1 {
    Q_OBJECT

public:
    explicit InputMethodContextV1(struct ::zwp_input_method_context_v1 *context, QObject *parent = nullptr);
    ~InputMethodContextV1() override;

    [[nodiscard]] quint32 serial() const { return m_serial; }
    [[nodiscard]] quint32 contentPurpose() const { return m_purpose; }
    [[nodiscard]] quint32 contentHint() const { return m_hint; }

    void commitText(const QString &text);
    void pressKeysym(quint32 keysym);

Q_SIGNALS:
    void contentTypeChanged();

protected:
    void zwp_input_method_context_v1_commit_state(uint32_t serial) override;
    void zwp_input_method_context_v1_content_type(uint32_t hint, uint32_t purpose) override;
    void zwp_input_method_context_v1_reset() override;

private:
    quint32 m_serial = 0;
    quint32 m_hint = 0;
    quint32 m_purpose = 0;
};

// The `zwp_input_method_v1` global only a compositor's input-method
// connection offers. Being active proves the keyboard was launched by KWin
// as its input method rather than by a curious user from a terminal.
class InputMethodV1 final : public QWaylandClientExtensionTemplate<InputMethodV1>,
                            public QtWayland::zwp_input_method_v1,
                            public KeyEmitter {
    Q_OBJECT

public:
    InputMethodV1();
    ~InputMethodV1() override;

    // Binds the global now instead of on the next event-loop turn.
    void bindNow() { initialize(); }
    [[nodiscard]] InputMethodContextV1 *context() const { return m_context.get(); }
    [[nodiscard]] bool activated() const { return m_context != nullptr; }

    void commitText(const QString &text) override;
    void pressKeysym(quint32 keysym) override;

Q_SIGNALS:
    void activatedChanged();

protected:
    void zwp_input_method_v1_activate(struct ::zwp_input_method_context_v1 *id) override;
    void zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context) override;

private:
    std::unique_ptr<InputMethodContextV1> m_context;
};

} // namespace QindaQt::Apps::Osk
