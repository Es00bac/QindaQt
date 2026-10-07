// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_renderer.h"
#include <poppler-qt6.h>
#include <algorithm>

namespace QindaQt::Viewer {
PageText DocumentRenderer::pageText(int pageNumber)
{
    PageText result;
    if (!m_pdf || m_pdf->isLocked()) {
        result.error = QStringLiteral("Text is available for unlocked PDF documents.");
        return result;
    }
    if (!m_pdf->okToCopy()) {
        result.error = QStringLiteral("This PDF does not allow text copying.");
        return result;
    }
    const auto page = m_pdf->page(pageNumber);
    if (!page) {
        result.error = QStringLiteral("This PDF page could not be read.");
        return result;
    }
    // AGENT-CONTRACT: Poppler remains worker-owned. ReadingOrder requires a
    // null rectangle. Results cross threads as plain strings, never HTML.
    result.text = page->text(QRectF(), Poppler::Page::ReadingOrder);
    if (result.text.size() > MaxPageText) {
        result.text.clear();
        result.error = QStringLiteral("This page has too much text to select or search safely.");
        return result;
    }
    result.available = true;
    return result;
}

TextSearchResult DocumentRenderer::find(
    const TextSearchRequest &request,
    const std::shared_ptr<std::atomic<quint64>> &latestSearch)
{
    TextSearchResult result;
    result.revision = request.revision;
    const auto current = [&] { return latestSearch->load() == request.revision; };
    if (!current()) return result;
    if (request.query.isEmpty() || request.query.size() > MaxSearchQuery) {
        result.status = TextSearchStatus::Limit;
        result.error = QStringLiteral("Enter between 1 and 512 characters to find.");
        return result;
    }
    RenderRequest document;
    document.path = request.path;
    document.password = request.password;
    result.error = load(document);
    if (!current()) return result;
    if (!result.error.isEmpty() || !m_pdf || m_pdf->isLocked() || !m_pdf->okToCopy()) {
        result.status = TextSearchStatus::Unavailable;
        if (result.error.isEmpty())
            result.error = QStringLiteral("Text search requires a PDF that allows copying.");
        return result;
    }
    const int pages = m_pdf->numPages();
    if (pages <= 0) {
        result.status = TextSearchStatus::Unavailable;
        result.error = QStringLiteral("This PDF has no pages.");
        return result;
    }
    const int firstPage = std::clamp(request.page, 0, pages - 1);
    const auto sensitivity = request.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QString firstText;
    int firstOffset = 0;
    const auto found = [&](int page, qsizetype start, const QString &text) {
        result.status = TextSearchStatus::Found;
        result.page = page;
        result.start = static_cast<int>(start);
        result.length = static_cast<int>(request.query.size());
        result.text = text;
    };
    const int visits = std::min(pages, MaxSearchPages);
    for (int visit = 0; visit < visits; ++visit) {
        if (!current()) return TextSearchResult{request.revision};
        const qint64 delta = request.backward ? -static_cast<qint64>(visit) : visit;
        const int page = static_cast<int>((static_cast<qint64>(firstPage) + delta + pages) % pages);
        auto text = pageText(page);
        if (!current()) return TextSearchResult{request.revision};
        if (!text.available) {
            result.status = TextSearchStatus::Limit;
            result.error = text.error;
            return result;
        }
        qsizetype start = -1;
        if (visit == 0) {
            firstText = text.text;
            firstOffset = std::clamp(request.offset, 0, static_cast<int>(text.text.size()));
            if (request.backward) {
                if (firstOffset > 0)
                    start = text.text.lastIndexOf(request.query, firstOffset - 1, sensitivity);
            } else {
                start = text.text.indexOf(request.query, firstOffset, sensitivity);
            }
        } else if (request.backward) {
            start = text.text.lastIndexOf(request.query, -1, sensitivity);
        } else {
            start = text.text.indexOf(request.query, 0, sensitivity);
        }
        if (start >= 0) {
            found(page, start, text.text);
            return result;
        }
    }
    if (pages > MaxSearchPages) {
        result.status = TextSearchStatus::Limit;
        result.error = QStringLiteral("Search stopped after 4096 pages. Continue from another page.");
        return result;
    }
    // Wrap the unsearched part of the first page without parsing it again.
    const qsizetype wrapped = request.backward
        ? firstText.lastIndexOf(request.query, -1, sensitivity)
        : firstText.indexOf(request.query, 0, sensitivity);
    if (wrapped >= 0 && (request.backward ? wrapped >= firstOffset : wrapped < firstOffset)) {
        found(firstPage, wrapped, firstText);
    } else {
        result.status = TextSearchStatus::NotFound;
    }
    return result;
}
} // namespace QindaQt::Viewer
