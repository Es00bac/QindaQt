// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
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

// Whether input may be delivered right now, and if not, why. The reason is
// sent to the phone (input.state), so it must be true: a state the source cannot
// confirm is Unverified, never Locked.
//
// AGENT-GUARD: fail closed. Open only on positive proof that the screen is
// unlocked and the session is in front. Locked only when the lock-state source
// says so; a missing or unauthenticated source is Unverified.
class InputGate : public QObject {
    Q_OBJECT
public:
    enum class Reason {
        Open,
        Locked,     // the lock-state source reports Locking or Locked
        Inactive,   // the session is not in front (reserved for a source that can report it)
        Unverified, // no usable lock-state source (Session1, compositor or attachment missing)
    };
    using QObject::QObject;
    [[nodiscard]] virtual Reason reason() const = 0;
    [[nodiscard]] bool open() const { return reason() == Reason::Open; }
    // The input.state string for a closed gate (protocol page, "Remote input").
    [[nodiscard]] static QString reasonName(Reason r)
    {
        switch (r) {
        case Reason::Locked: return QStringLiteral("locked");
        case Reason::Inactive: return QStringLiteral("inactive");
        case Reason::Unverified: return QStringLiteral("unverified");
        case Reason::Open: break;
        }
        return QStringLiteral("active");
    }
Q_SIGNALS:
    void changed();
};

// Test and fallback gate with a settable state.
class StaticInputGate final : public InputGate {
    Q_OBJECT
public:
    explicit StaticInputGate(bool open = true, QObject *parent = nullptr)
        : InputGate(parent), m_reason(open ? Reason::Open : Reason::Unverified) {}
    [[nodiscard]] Reason reason() const override { return m_reason; }
    // false means Locked, the case a test usually wants.
    void setOpen(bool open) { setReason(open ? Reason::Open : Reason::Locked); }
    void setReason(Reason r)
    {
        if (m_reason != r) {
            m_reason = r;
            Q_EMIT changed();
        }
    }

private:
    Reason m_reason;
};

// Wraps a sink and remembers what this owner holds down, so that releaseAll()
// can never leave a key or button stuck.
//
// A press is remembered only when the sink accepted it, and a release forgets
// it only when the sink accepted that: a failed key-up stays in the ledger so
// the watchdog or the next releaseAll() retries it.
// AGENT-GUARD: every feature that presses something must press it through a
// HeldInput of its own. A disconnect, lock, cancel or begin calls releaseAll();
// a missed path leaves a key or button stuck down on the desktop.
class HeldInput final : public InputSink {
public:
    explicit HeldInput(InputSink &sink) : m_sink(sink) { m_clock.start(); }
    [[nodiscard]] bool active() const override { return m_sink.active(); }
    bool pointerMotion(double dx, double dy) override { return m_sink.pointerMotion(dx, dy); }
    bool pointerButton(qint32 code, bool down) override
    {
        const bool ok = m_sink.pointerButton(code, down);
        if (down && ok) m_buttons.insert(code, m_clock.elapsed());
        else if (!down && ok) m_buttons.remove(code);
        return ok;
    }
    bool pointerAxis(double dx, double dy, bool finish) override { return m_sink.pointerAxis(dx, dy, finish); }
    bool pointerAxisDiscrete(quint32 axis, qint32 steps) override { return m_sink.pointerAxisDiscrete(axis, steps); }
    bool keysym(qint32 sym, bool down) override
    {
        const bool ok = m_sink.keysym(sym, down);
        if (down && ok) m_keys.insert(sym, m_clock.elapsed());
        else if (!down && ok) m_keys.remove(sym);
        return ok;
    }
    [[nodiscard]] bool holding() const { return !m_keys.isEmpty() || !m_buttons.isEmpty(); }
    // Releases everything this owner holds (failures stay for a retry).
    void releaseAll() { releaseOlderThan(-1); }
    // Releases what has been held for more than `ms` without being pressed again.
    void releaseOlderThan(qint64 ms)
    {
        const qint64 now = m_clock.elapsed();
        const auto buttons = m_buttons.keys();
        for (qint32 b : buttons) {
            if (ms < 0 || now - m_buttons.value(b) > ms) pointerButton(b, false);
        }
        const auto keys = m_keys.keys();
        for (qint32 k : keys) {
            if (ms < 0 || now - m_keys.value(k) > ms) keysym(k, false);
        }
    }

private:
    InputSink &m_sink;
    QElapsedTimer m_clock;
    QHash<qint32, qint64> m_keys, m_buttons; // value: elapsed ms at the last press
};

} // namespace QindaQt::Link
