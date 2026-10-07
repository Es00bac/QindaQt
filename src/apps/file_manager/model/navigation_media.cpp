// SPDX-License-Identifier: GPL-3.0-or-later
#include "navigation_controller.h"
namespace QindaQt::Apps::FileManager {
void NavigationController::clearMediaRevocation() {
    if (!m_mediaLocationRevoked) return;
    m_mediaLocationRevoked = false;
    emit presentationChanged();
}
void NavigationController::invalidateMediaLocation(const QString &message) {
    if (m_mediaLocationRevoked) return;
    m_mediaLocationRevoked = true;
    ++m_listingGeneration;
    m_guestActive = false;
    m_guestStatusText.clear();
    m_listedEntries.clear();
    m_entries.clear();
    m_nameFilter.clear();
    m_hiddenFilteredCount = 0;
    m_truncated = false;
    m_status = NavigationStatus::Unavailable;
    m_statusMessage = message;
    // Selection/actions observe an empty new generation before any stale
    // search/listing producer can publish a reused mount pathname.
    emit entriesChanged();
    emit presentationChanged();
    emit navigationChanged();
}
}
