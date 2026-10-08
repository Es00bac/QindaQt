// SPDX-License-Identifier: GPL-3.0-or-later
#include "clipboard_privacy_state_p.h"

#include <utility>

namespace QindaQt::Services::Clipboard {

ClipboardPrivacyState::ClipboardPrivacyState(
    ClipboardWayland::ClipboardWaylandAdapter &adapter, PrivacyAdmission admission,
    Change changed)
    : m_adapter(adapter), m_admission(std::move(admission)), m_changed(std::move(changed))
{
}

bool ClipboardPrivacyState::evaluateAdmission()
{
    if (m_closing || m_evaluating || !m_admission) {
        if (m_evaluating) m_evaluationRevoked = true;
        return false;
    }
    m_evaluating = true;
    m_evaluationRevoked = false;
    bool admitted = false;
    try {
        admitted = m_admission();
    } catch (...) {
        // A borrowed authority callback is a denial boundary, not an exception
        // path that may retain previously admitted clipboard content.
    }
    m_evaluating = false;
    return admitted && !m_evaluationRevoked && !m_closing;
}

void ClipboardPrivacyState::deny()
{
    const auto before = history.snapshot();
    // AGENT-GUARD: revoke model lineage BEFORE adapter cancellation or Changed.
    // Both can reenter the host, and cancellation may delete the captured()
    // argument because it aliases adapter transfer storage. The caller must
    // return without inspecting that argument after a denied check.
    m_unlocked = false;
    history.setPrivacyAllowed(false);
    if (m_closing) return;
    m_closing = true;
    m_adapter.setCaptureEnabled(false);
    m_changed(before);
    m_closing = false;
}

bool ClipboardPrivacyState::check()
{
    if (m_closing) return false;
    if (!m_unlocked) return false;
    if (!evaluateAdmission() || !m_unlocked) {
        deny();
        return false;
    }
    return true;
}

void ClipboardPrivacyState::setUnlocked(bool allowed)
{
    if (!allowed || !evaluateAdmission()) {
        deny();
        return;
    }
    const auto before = history.snapshot();
    m_unlocked = true;
    history.setPrivacyAllowed(true);
    synchronizeCapture();
    m_changed(before);
    // A Changed receiver may synchronously revoke the attachment.
    (void)check();
}

void ClipboardPrivacyState::setHistoryOptIn(bool enabled)
{
    const auto before = history.snapshot();
    history.setHistoryEnabled(enabled);
    synchronizeCapture();
    m_changed(before);
    (void)check();
}

void ClipboardPrivacyState::synchronizeCapture()
{
    if (m_synchronizing || m_closing) return;
    m_synchronizing = true;
    const bool allowed = check();
    const bool available = allowed && m_adapter.isAvailable();
    // The adapter is external; its availability/capture methods may synchronously
    // change consent or authority. Do not retain the earlier decision.
    const bool enable = available && check() && history.isHistoryEnabled();
    m_adapter.setCaptureEnabled(enable);
    if (!check() || !history.isHistoryEnabled())
        m_adapter.setCaptureEnabled(false);
    m_synchronizing = false;
}

void ClipboardPrivacyState::synchronizeCapture(bool available)
{
    if (!available) {
        m_adapter.setCaptureEnabled(false);
        (void)check();
        return;
    }
    synchronizeCapture();
}

} // namespace QindaQt::Services::Clipboard
