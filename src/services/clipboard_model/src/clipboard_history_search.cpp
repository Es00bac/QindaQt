// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_history.h>

#include <QtCore/QtGlobal>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

namespace QindaQt::Services::ClipboardModel {

SearchOutcome ClipboardHistoryModel::search(const QString &query,
                                            quint32 expectedGeneration,
                                            int maxResults) const
{
    SearchOutcome outcome;
    const Gate gate = gateOperation(expectedGeneration);
    if (gate.refused) {
        outcome.error = gate.error;
        return outcome;
    }
    if (query.isEmpty()) {
        outcome.error = ClipboardError::EmptyValue;
        return outcome;
    }
    if (query.size() > kMaxPreviewCodeUnits) {
        outcome.error = ClipboardError::OversizedValue;
        return outcome;
    }
    // maxResults is sanitized, not refused: the interesting bound is the
    // history itself, and a caller asking for "all" passes kMaxEntries.
    const int cappedResults = qBound(1, maxResults, kMaxEntries);

    // AGENT-CONTRACT: search sees only the two bounded metadata fields —
    // preview and sanitized source label. Payload bytes are unreachable
    // here, so a future presentation layer can offer search without payload
    // authority; broader content search semantics belong to the C1 slice.
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
    for (const Entry &entry : m_entries) {
        const bool matches =
            entry.descriptor.preview.contains(query, sensitivity)
            || entry.descriptor.sourceLabel.contains(query, sensitivity);
        if (!matches) {
            continue;
        }
        if (outcome.matches.size() == cappedResults) {
            outcome.truncated = true;
            break;
        }
        outcome.matches.append(entry.descriptor);
    }
    return outcome;
}

} // namespace QindaQt::Services::ClipboardModel
