// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/clipboard_applet/clipboard_applet_model.h"

#include <QtCore/QLocale>

namespace QindaQt::ShellClipboardApplet {

QString ClipboardAppletModel::formatByteSize(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("0 B");
    }
    if (bytes < 1024) {
        return QString::asprintf("%lld B", static_cast<long long>(bytes));
    }
    if (bytes < 1024 * 1024) {
        const double kb = static_cast<double>(bytes) / 1024.0;
        return QString::asprintf("%.1f KB", kb);
    }
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QString::asprintf("%.1f MB", mb);
}

QString ClipboardAppletModel::formatSummary(
    const QList<QindaQt::Services::ClipboardModel::FormatInfo> &formats,
    qint64 totalBytes)
{
    if (formats.isEmpty()) {
        return QStringLiteral("No formats");
    }
    const QString firstType = formats.first().mediaType;
    const QString sizeStr = formatByteSize(totalBytes);
    if (formats.size() == 1) {
        return QStringLiteral("%1 (%2)").arg(firstType, sizeStr);
    }
    return QStringLiteral("%1 (+%2 formats, %3)")
        .arg(firstType)
        .arg(formats.size() - 1)
        .arg(sizeStr);
}

QString ClipboardAppletModel::accessibleNameForRow(
    int index,
    const QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor &desc)
{
    QString details;
    if (desc.pinned) {
        details += QStringLiteral(", pinned");
    }
    if (!desc.sourceLabel.isEmpty()) {
        details += QStringLiteral(", from %1").arg(desc.sourceLabel);
    }
    const QString primaryType = desc.formats.isEmpty()
        ? QStringLiteral("empty")
        : desc.formats.first().mediaType;
    const QString previewSnippet = desc.preview.isEmpty()
        ? QStringLiteral("(empty)")
        : desc.preview;

    return QStringLiteral("Entry %1: %2%3, preview: %4")
        .arg(index + 1)
        .arg(primaryType)
        .arg(details)
        .arg(previewSnippet);
}

ClipboardEntryRow ClipboardAppletModel::projectRow(
    int index,
    const QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor &desc,
    bool isPending)
{
    ClipboardEntryRow row;
    row.generation = desc.id.generation;
    row.serial = desc.id.serial;
    row.idString = QString::asprintf("%u:%u", desc.id.generation, desc.id.serial);
    row.preview = desc.preview;
    row.previewTruncated = desc.previewTruncated;
    row.sourceLabel = desc.sourceLabel;
    row.pinned = desc.pinned;
    row.admittedTick = desc.admittedTick;
    row.lastUsedTick = desc.lastUsedTick;
    row.pending = isPending;

    qint64 totalBytes = 0;
    for (const auto &fmt : desc.formats) {
        totalBytes += qMax<qint64>(0, fmt.payloadBytes);
    }
    row.totalBytes = totalBytes;
    row.formatsSummary = formatSummary(desc.formats, totalBytes);

    if (!desc.formats.isEmpty()) {
        const QString &firstType = desc.formats.first().mediaType;
        row.primaryMediaType = firstType;
        if (firstType == QLatin1String("text/uri-list")) {
            row.isUriList = true;
        } else if (firstType.startsWith(QLatin1String("image/"))) {
            row.isImage = true;
        } else if (firstType == QLatin1String("text/plain") || firstType == QLatin1String("text/html")
                   || firstType.startsWith(QLatin1String("text/"))) {
            row.isText = true;
        }
    } else {
        row.primaryMediaType = QStringLiteral("application/octet-stream");
    }

    row.accessibleName = accessibleNameForRow(index, desc);
    row.accessibleDescription = QStringLiteral("%1; size: %2; id: %3")
        .arg(row.formatsSummary, formatByteSize(totalBytes), row.idString);

    return row;
}

ClipboardAppletProjection ClipboardAppletModel::project(
    const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot,
    ClientState clientState,
    const QString &clientReasonCode,
    bool ownerAvailable,
    bool isLocked,
    bool isSearchActive,
    const QString &searchQuery,
    const QList<QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor> &searchResults,
    bool searchTruncated,
    const QSet<QPair<quint32, quint32>> &pendingEntries)
{
    ClipboardAppletProjection proj;

    // 1. Phase Determination (Fail-closed ordering)
    if (!ownerAvailable || clientState == ClientState::Unavailable) {
        proj.phase = Phase::Unavailable;
        proj.phaseReasonText = clientReasonCode.isEmpty()
            ? QStringLiteral("Clipboard service is currently unavailable.")
            : QStringLiteral("Clipboard service unavailable: %1").arg(clientReasonCode);
    } else if (clientState == ClientState::Stopped || clientState == ClientState::Starting) {
        proj.phase = Phase::Loading;
        proj.phaseReasonText = QStringLiteral("Connecting to clipboard service…");
    } else if (isLocked || !snapshot.privacyAllowed) {
        proj.phase = Phase::Locked;
        proj.phaseReasonText = QStringLiteral("Clipboard history is hidden while the session is locked.");
    } else if (!snapshot.historyEnabled) {
        proj.phase = Phase::Disabled;
        proj.phaseReasonText = QStringLiteral("Clipboard history is disabled.");
    } else if (clientState == ClientState::Degraded) {
        proj.phase = Phase::Degraded;
        proj.phaseReasonText = clientReasonCode.isEmpty()
            ? QStringLiteral("Clipboard service is running with limited functionality.")
            : QStringLiteral("Clipboard service degraded: %1").arg(clientReasonCode);
    } else {
        proj.phase = Phase::Ready;
    }

    // If not in an active viewing phase, fail closed and withhold entries
    if (proj.phase != Phase::Ready && proj.phase != Phase::Degraded) {
        proj.emptyReasonText = proj.phaseReasonText;
        return proj;
    }

    // 2. Count statistics from the underlying snapshot
    int pinned = 0;
    int unpinned = 0;
    for (const auto &entry : snapshot.entries) {
        if (entry.pinned) {
            ++pinned;
        } else {
            ++unpinned;
        }
    }
    proj.pinnedCount = pinned;
    proj.unpinnedCount = unpinned;
    proj.totalPayloadBytes = snapshot.totalPayloadBytes;
    proj.totalPayloadBytesFormatted = formatByteSize(snapshot.totalPayloadBytes);

    // 3. Select list source (search results vs full history)
    const auto &sourceList = isSearchActive ? searchResults : snapshot.entries;
    proj.isSearchActive = isSearchActive;
    proj.searchQuery = searchQuery;
    proj.searchResultCount = isSearchActive ? static_cast<int>(searchResults.size()) : 0;
    proj.searchTruncated = isSearchActive && searchTruncated;

    const int count = qMin<int>(static_cast<int>(sourceList.size()), kMaxPresentedEntries);
    proj.entryRows.reserve(count);
    for (int i = 0; i < count; ++i) {
        const auto &desc = sourceList.at(i);
        const bool pending = pendingEntries.contains({desc.id.generation, desc.id.serial});
        proj.entryRows.append(projectRow(i, desc, pending));
    }

    // 4. Empty Reason Text
    if (proj.entryRows.isEmpty()) {
        if (isSearchActive) {
            proj.emptyReasonText = QStringLiteral("No matching items found for \"%1\"").arg(searchQuery);
        } else {
            proj.emptyReasonText = QStringLiteral("Clipboard history is empty.");
        }
    }

    return proj;
}

} // namespace QindaQt::ShellClipboardApplet
