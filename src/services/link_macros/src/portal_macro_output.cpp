// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_macros/keysym_map.h>
#include <qindaqt/services/link_macros/portal_macro_output.h>

namespace QindaQt::Link::Macros {
namespace {
using InputActions::Binding;

qint32 evdevButton(const QString &name)
{
    if (name == QLatin1String("left")) return 272;
    if (name == QLatin1String("right")) return 273;
    if (name == QLatin1String("middle")) return 274;
    if (name == QLatin1String("back")) return 275;    // BTN_SIDE
    if (name == QLatin1String("forward")) return 276; // BTN_EXTRA
    return 0;
}

void addUnique(QList<qint32> &list, qint32 v)
{
    if (!list.contains(v)) list.append(v);
}
}

PortalMacroOutput::PortalMacroOutput(InputSink &sink, InputGate &gate, QObject *parent)
    : QObject(parent), m_sink(sink), m_gate(gate), m_text(sink)
{
    m_nudgeTimer.setInterval(60);
    connect(&m_nudgeTimer, &QTimer::timeout, this, [this] {
        if (!allowed()) {
            reset({});
            return;
        }
        for (const QPointF &d : std::as_const(m_nudges)) m_sink.pointerMotion(d.x(), d.y());
    });
    // A lock releases everything at once, not at the runner's next tick.
    connect(&m_gate, &InputGate::changed, this, [this] {
        if (!m_gate.open()) reset({});
    });
}

PortalMacroOutput::~PortalMacroOutput() { reset({}); }

bool PortalMacroOutput::resolve(const Binding &b, Hold &hold, bool &nudge, QPointF &delta) const
{
    nudge = false;
    const QString &a = b.action;
    if (a.startsWith(QLatin1String("pointer-"))) {
        static const QHash<QString, QPointF> dirs{{QStringLiteral("pointer-left"), {-10, 0}}, {QStringLiteral("pointer-right"), {10, 0}},
                                                  {QStringLiteral("pointer-up"), {0, -10}}, {QStringLiteral("pointer-down"), {0, 10}}};
        if (!dirs.contains(a)) return false;
        delta = dirs.value(a);
        nudge = true;
        return true;
    }
    if (a == QLatin1String("combination")) {
        for (const QString &m : b.modifiers) {
            const qint32 sym = modifierKeysym(m);
            if (sym == 0) return false;
            addUnique(hold.keys, sym);
        }
        for (const QString &k : b.keys) {
            const QList<qint32> chord = keysymsForChord(k);
            if (chord.isEmpty()) return false; // resolve everything before pressing anything
            for (qint32 s : chord) addUnique(hold.keys, s);
        }
        for (const QString &m : b.mouseButtons) {
            const qint32 code = evdevButton(m);
            if (code == 0) return false;
            addUnique(hold.buttons, code);
        }
        return !hold.keys.isEmpty() || !hold.buttons.isEmpty();
    }
    if (a == QLatin1String("left-click") || a == QLatin1String("left-click-meta-shift")) hold.buttons = {272};
    else if (a == QLatin1String("right-click") || a == QLatin1String("right-click-meta-shift")) hold.buttons = {273};
    else if (a == QLatin1String("middle-click")) hold.buttons = {274};
    else {
        const QString chord = a == QLatin1String("shortcut") ? b.shortcut : defaultChordForAction(a);
        hold.keys = keysymsForChord(chord);
        return !hold.keys.isEmpty();
    }
    if (a.endsWith(QLatin1String("-meta-shift"))) hold.keys = {modifierKeysym(QStringLiteral("Meta")), modifierKeysym(QStringLiteral("Shift"))};
    return true;
}

bool PortalMacroOutput::invoke(const QString &token, const Binding &binding, bool down)
{
    if (!down) {
        release(token); // a text token holds nothing, so this is a no-op for it
        return true;
    }
    if (!allowed()) return false;
    if (m_holds.contains(token) || m_nudges.contains(token) || binding.action == QLatin1String("none")) return true;
    if (binding.action == QLatin1String("text")) return m_text.enqueue(token, binding.text);
    Hold hold;
    bool nudge = false;
    QPointF delta;
    if (!resolve(binding, hold, nudge, delta)) return false;
    if (nudge) {
        m_sink.pointerMotion(delta.x(), delta.y());
        m_nudges.insert(token, delta);
        m_nudgeTimer.start();
        return true;
    }
    press(token, hold);
    return true;
}

void PortalMacroOutput::press(const QString &token, const Hold &hold)
{
    m_holds.insert(token, hold);
    for (qint32 k : hold.keys) {
        if (m_keyCounts[k]++ == 0) m_sink.keysym(k, true);
    }
    for (qint32 b : hold.buttons) {
        if (m_buttonCounts[b]++ == 0) m_sink.pointerButton(b, true);
    }
}

void PortalMacroOutput::release(const QString &token)
{
    if (m_nudges.remove(token) > 0 && m_nudges.isEmpty()) m_nudgeTimer.stop();
    const auto it = m_holds.find(token);
    if (it == m_holds.end()) return;
    const Hold hold = it.value();
    m_holds.erase(it);
    for (auto b = hold.buttons.crbegin(); b != hold.buttons.crend(); ++b) {
        if (--m_buttonCounts[*b] <= 0) {
            m_buttonCounts.remove(*b);
            m_sink.pointerButton(*b, false);
        }
    }
    for (auto k = hold.keys.crbegin(); k != hold.keys.crend(); ++k) {
        if (--m_keyCounts[*k] <= 0) {
            m_keyCounts.remove(*k);
            m_sink.keysym(*k, false);
        }
    }
}

void PortalMacroOutput::motion(QPointF delta)
{
    if (!delta.isNull() && allowed()) m_sink.pointerMotion(delta.x(), delta.y());
}

void PortalMacroOutput::scroll(QPointF delta)
{
    if (!delta.isNull() && allowed()) m_sink.pointerAxis(delta.x(), delta.y(), true);
}

void PortalMacroOutput::reset(const QString &prefix)
{
    m_text.cancel(prefix);
    const auto matches = [&prefix](const QString &t) { return prefix.isEmpty() || t == prefix || t.startsWith(prefix + QLatin1Char('/')); };
    const QStringList tokens = m_holds.keys() + m_nudges.keys();
    for (const QString &t : tokens) {
        if (matches(t)) release(t);
    }
}

InputActions::MacroOutput::TextState PortalMacroOutput::textState(const QString &token) const
{
    switch (m_text.state(token)) {
    case TextTyper::State::Pending: return Pending;
    case TextTyper::State::Complete: return Complete;
    default: return Failed;
    }
}

} // namespace QindaQt::Link::Macros
