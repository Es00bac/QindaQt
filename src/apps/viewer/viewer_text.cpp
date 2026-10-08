// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewer_controller.h"
#include <QMetaObject>

namespace QindaQt::Viewer {
void ViewerController::resetSearch()
{
    ++(*m_latestSearch);
    m_searchBusy = false;
    m_searchMessage.clear();
    m_matchStart = -1;
    m_matchLength = 0;
    m_lastQuery.clear();
    m_matchText.clear();
}
void ViewerController::cancelSearch()
{
    resetSearch();
    emit stateChanged();
}
void ViewerController::find(const QString &query, bool backward, bool caseSensitive)
{
    if (!ready() || !m_pdf || !m_textAllowed || m_busy || m_searchBusy) return;
    if (query.isEmpty() || query.size() > MaxSearchQuery) {
        resetSearch();
        m_searchMessage = tr("Enter between 1 and 512 characters to find.");
        emit stateChanged();
        return;
    }
    const bool continuing = query == m_lastQuery && caseSensitive == m_lastCaseSensitive
        && m_matchStart >= 0;
    TextSearchRequest request;
    request.revision = ++(*m_latestSearch);
    request.path = m_request.path;
    request.password = m_request.password;
    request.query = query;
    request.page = m_page;
    request.offset = continuing
        ? (backward ? m_matchStart : m_matchStart + m_matchLength)
        : (backward ? static_cast<int>(m_pageText.size()) : 0);
    request.backward = backward;
    request.caseSensitive = caseSensitive;
    m_lastQuery = query;
    m_lastCaseSensitive = caseSensitive;
    m_searchBusy = true;
    m_searchMessage = tr("Searching…");
    m_matchStart = -1;
    m_matchLength = 0;
    m_matchText.clear();
    emit stateChanged();
    const auto renderer = m_renderer;
    const auto latestSearch = m_latestSearch;
    // AGENT-GUARD: search lifetime is separate from raster zoom. Close,
    // replacement, manual navigation and explicit cancel retire disclosure.
    QMetaObject::invokeMethod(m_worker, [this, renderer, latestSearch, request] {
        auto result = renderer->find(request, latestSearch);
        QMetaObject::invokeMethod(this, [this, result = std::move(result)]() mutable {
            acceptSearch(std::move(result));
        });
    });
}
void ViewerController::acceptSearch(TextSearchResult result)
{
    if (result.revision != m_latestSearch->load()) return;
    m_searchBusy = false;
    if (result.status == TextSearchStatus::Found) {
        m_matchStart = result.start;
        m_matchLength = result.length;
        m_matchText = std::move(result.text);
        m_searchMessage = tr("Found on page %1.").arg(result.page + 1);
        if (result.page != m_page) {
            m_page = result.page;
            m_request.page = m_page;
            requestRender();
        }
    } else if (result.status == TextSearchStatus::NotFound) {
        m_searchMessage = tr("No matches found.");
    } else if (result.status != TextSearchStatus::Cancelled) {
        m_searchMessage = result.error;
    } else {
        m_searchMessage.clear();
    }
    emit stateChanged();
}
} // namespace QindaQt::Viewer
