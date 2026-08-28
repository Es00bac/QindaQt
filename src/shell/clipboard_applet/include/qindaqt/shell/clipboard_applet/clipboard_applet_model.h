// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QSet>
#include <QtCore/QPair>
#include <qindaqt/services/clipboard_model/clipboard_types.h>
#include "qindaqt/shell/clipboard_applet/clipboard_applet_types.h"

namespace QindaQt::ShellClipboardApplet {

// AGENT-CONTRACT: Pure functional presentation model projecting snapshot state
// and client facts into immutable, bounded UI structures for QML presentation.
// This class owns no state, handles, or threads; it executes deterministic
// projections only.
class ClipboardAppletModel final {
public:
    ClipboardAppletModel() = delete;

    [[nodiscard]] static ClipboardAppletProjection project(
        const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot,
        ClientState clientState,
        const QString &clientReasonCode,
        bool ownerAvailable,
        bool isLocked,
        bool isSearchActive,
        const QString &searchQuery,
        const QList<QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor> &searchResults,
        bool searchTruncated,
        const QSet<QPair<quint32, quint32>> &pendingEntries);

    [[nodiscard]] static ClipboardEntryRow projectRow(
        int index,
        const QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor &desc,
        bool isPending);

    [[nodiscard]] static QString formatByteSize(qint64 bytes);
    [[nodiscard]] static QString formatSummary(
        const QList<QindaQt::Services::ClipboardModel::FormatInfo> &formats,
        qint64 totalBytes);
    [[nodiscard]] static QString accessibleNameForRow(
        int index,
        const QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor &desc);
};

} // namespace QindaQt::ShellClipboardApplet
