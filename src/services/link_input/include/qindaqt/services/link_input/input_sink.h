// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QtGlobal>

namespace QindaQt::Link {

// The one place desktop input leaves QindaLink. Reusable by any later feature
// (the vault types through this same interface, ADR-0436).
//
// Ownership: implementations are owned by the service composition and outlive
// every feature that borrows them. Threading: the GUI thread only.
// Error behavior: every method returns false, and sends nothing, while
// active() is false or when the transport call fails.
// AGENT-CONTRACT: keysyms are X11 keysyms; Unicode is 0x01000000 | codepoint
// above U+00FF. Button codes are Linux evdev codes (BTN_LEFT 272 ...).
class InputSink {
public:
    virtual ~InputSink() = default;
    [[nodiscard]] virtual bool active() const = 0;
    virtual bool pointerMotion(double dx, double dy) = 0;
    virtual bool pointerButton(qint32 evdevCode, bool down) = 0;
    // Smooth scrolling in portal axis units; `finish` ends a gesture.
    virtual bool pointerAxis(double dx, double dy, bool finish) = 0;
    // axis: 0 vertical, 1 horizontal.
    virtual bool pointerAxisDiscrete(quint32 axis, qint32 steps) = 0;
    virtual bool keysym(qint32 keysym, bool down) = 0;
};

// Whether input may be delivered right now (the screen is unlocked and the
// session is in front). Fail closed: an unknown state is closed.
class InputGate : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual bool open() const = 0;
Q_SIGNALS:
    void changed();
};

// Test and fallback gate with a settable state.
class StaticInputGate final : public InputGate {
    Q_OBJECT
public:
    explicit StaticInputGate(bool open = true, QObject *parent = nullptr) : InputGate(parent), m_open(open) {}
    [[nodiscard]] bool open() const override { return m_open; }
    void setOpen(bool open)
    {
        if (m_open != open) {
            m_open = open;
            Q_EMIT changed();
        }
    }

private:
    bool m_open;
};

// Wraps a sink and remembers what this owner holds down, so that releaseAll()
// can never leave a key or button stuck.
//
// AGENT-GUARD: every feature that presses something must press it through a
// HeldInput of its own. A disconnect, lock or cancel calls releaseAll().
class HeldInput final : public InputSink {
public:
    explicit HeldInput(InputSink &sink) : m_sink(sink) {}
    [[nodiscard]] bool active() const override { return m_sink.active(); }
    bool pointerMotion(double dx, double dy) override { return m_sink.pointerMotion(dx, dy); }
    bool pointerButton(qint32 code, bool down) override
    {
        if (down) m_buttons.insert(code); else m_buttons.remove(code);
        return m_sink.pointerButton(code, down);
    }
    bool pointerAxis(double dx, double dy, bool finish) override { return m_sink.pointerAxis(dx, dy, finish); }
    bool pointerAxisDiscrete(quint32 axis, qint32 steps) override { return m_sink.pointerAxisDiscrete(axis, steps); }
    bool keysym(qint32 sym, bool down) override
    {
        if (down) m_keys.insert(sym); else m_keys.remove(sym);
        return m_sink.keysym(sym, down);
    }
    [[nodiscard]] bool holding() const { return !m_keys.isEmpty() || !m_buttons.isEmpty(); }
    void releaseAll()
    {
        const QSet<qint32> keys = std::exchange(m_keys, {});
        const QSet<qint32> buttons = std::exchange(m_buttons, {});
        for (qint32 b : buttons) m_sink.pointerButton(b, false);
        for (qint32 k : keys) m_sink.keysym(k, false);
    }

private:
    InputSink &m_sink;
    QSet<qint32> m_keys, m_buttons;
};

} // namespace QindaQt::Link
