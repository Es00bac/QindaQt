// SPDX-License-Identifier: GPL-3.0-or-later
#include "button_layers_p.h"
namespace QindaQt::Controllers {
namespace {
QString layer(const Binding &binding) { return binding.action == "hold-page" ? binding.page : binding.holdPage; }
}
const ButtonPage &ButtonLayers::page(const Profile &profile) const {
    if (!m_modifiers.isEmpty()) {
        const auto id = layer(m_held.value(m_modifiers.last()));
        for (const auto &page : profile.pages) if (page.id == id) return page;
    }
    return activePage(profile);
}
bool ButtonLayers::heldAction(const QString &action) const {
    for (const auto &binding : m_held) if (binding.action == action) return true;
    return false;
}
void ButtonLayers::clear() { m_held.clear(); m_physical.clear(); m_modifiers.clear(); m_quarantined.clear(); }
QList<ButtonEdge> ButtonLayers::sample(const Profile &profile, const QMap<QString, bool> &physical) {
    QList<ButtonEdge> edges;
    const auto oldLayer = page(profile).id;
    for (const auto &button : m_physical) if (!physical.value(button)) {
        if (m_held.contains(button)) edges.append({button, m_held.take(button), false});
        m_modifiers.removeAll(button); m_quarantined.remove(button);
    }
    // Resolve base-page layer activators before other simultaneous presses.
    // A layer never rebinds its own modifier and cannot leave it stuck down.
    for (const auto &button : buttonIds()) if (physical.value(button) && !m_physical.contains(button)) {
        const auto binding = activePage(profile).bindings.value(button);
        if (layer(binding).isEmpty()) continue;
        m_held[button] = binding; m_modifiers.append(button); edges.append({button, binding, true});
    }
    if (oldLayer != page(profile).id) {
        for (const auto &button : m_held.keys()) {
            if (m_modifiers.contains(button) || m_held.value(button).action == "hold-gyro") continue;
            // AGENT-GUARD: a pre-held D-pad/button must be released before it
            // can issue a different layer's action. Modifier/gyro contact holds
            // stay alive; unrelated owned keys and clicks release immediately.
            edges.append({button, m_held.take(button), false}); m_quarantined.insert(button);
        }
    }
    for (const auto &button : buttonIds()) if (physical.value(button) && !m_physical.contains(button)
        && !m_modifiers.contains(button) && !m_quarantined.contains(button)) {
        const auto binding = page(profile).bindings.value(button);
        m_held[button] = binding; edges.append({button, binding, true});
    }
    m_physical.clear(); for (auto it = physical.begin(); it != physical.end(); ++it) if (it.value()) m_physical.insert(it.key());
    return edges;
}
} // namespace QindaQt::Controllers
