// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_method_client.h"

#include <QDateTime>

namespace QindaQt::Apps::Osk {

InputMethodContextV1::InputMethodContextV1(struct ::zwp_input_method_context_v1 *context, QObject *parent)
    : QObject(parent), QtWayland::zwp_input_method_context_v1(context)
{
}

InputMethodContextV1::~InputMethodContextV1()
{
    if (isInitialized()) {
        destroy();
    }
}

void InputMethodContextV1::commitText(const QString &text)
{
    commit_string(m_serial, text);
}

void InputMethodContextV1::pressKeysym(quint32 keysym)
{
    // KWin acts on the press and synthesises the release itself for
    // text-input v3 clients; sending both keeps v1/v2 clients whole.
    const auto time = static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() & 0xffffffffu);
    this->keysym(m_serial, time, keysym, 1, 0);
    this->keysym(m_serial, time, keysym, 0, 0);
}

void InputMethodContextV1::zwp_input_method_context_v1_commit_state(uint32_t serial)
{
    m_serial = serial;
}

void InputMethodContextV1::zwp_input_method_context_v1_content_type(uint32_t hint, uint32_t purpose)
{
    m_hint = hint;
    m_purpose = purpose;
    Q_EMIT contentTypeChanged();
}

void InputMethodContextV1::zwp_input_method_context_v1_reset()
{
}

InputMethodV1::InputMethodV1() : QWaylandClientExtensionTemplate<InputMethodV1>(1) {}

InputMethodV1::~InputMethodV1()
{
    m_context.reset();
}

void InputMethodV1::commitText(const QString &text)
{
    if (m_context) {
        m_context->commitText(text);
    }
}

void InputMethodV1::pressKeysym(quint32 keysym)
{
    if (m_context) {
        m_context->pressKeysym(keysym);
    }
}

void InputMethodV1::zwp_input_method_v1_activate(struct ::zwp_input_method_context_v1 *id)
{
    m_context = std::make_unique<InputMethodContextV1>(id);
    Q_EMIT activatedChanged();
}

void InputMethodV1::zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context)
{
    if (m_context && m_context->object() == context) {
        m_context.reset();
        Q_EMIT activatedChanged();
    }
}

} // namespace QindaQt::Apps::Osk
