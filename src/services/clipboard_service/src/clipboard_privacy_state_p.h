// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/clipboard_service/clipboard_host.h>

#include <functional>

namespace QindaQt::Services::Clipboard {

// Private same-thread authority state. A const host snapshot may reconcile
// revocation through this owned pointee; payload storage never becomes const
// mutable state or a detached overlay with unchanged protocol lineage.
class ClipboardPrivacyState final {
public:
    using Change = std::function<void(const ClipboardModel::HistorySnapshot &)>;
    ClipboardPrivacyState(ClipboardWayland::ClipboardWaylandAdapter &adapter,
                          PrivacyAdmission admission, Change changed);
    [[nodiscard]] bool check();
    void setUnlocked(bool allowed);
    void setHistoryOptIn(bool enabled);
    void synchronizeCapture(bool available);
    ClipboardModel::ClipboardHistoryModel history;

private:
    [[nodiscard]] bool evaluateAdmission();
    void deny();
    void synchronizeCapture();
    ClipboardWayland::ClipboardWaylandAdapter &m_adapter;
    PrivacyAdmission m_admission;
    Change m_changed;
    bool m_unlocked = false;
    bool m_evaluating = false;
    bool m_evaluationRevoked = false;
    bool m_closing = false;
    bool m_synchronizing = false;
};

} // namespace QindaQt::Services::Clipboard
