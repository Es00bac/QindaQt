// SPDX-License-Identifier: GPL-3.0-or-later

// The Voice console's session history: what was dictated, where it went, and
// when. Separate from the live projection because its rules are its own.

#include "voice_console_model.h"

#include <qindaqt/services/voice_client/voice_client.h>

#include <QtCore/QTime>
#include <QtCore/QVariantMap>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace QindaQt::Apps::Voice {

using Services::Voice::CaptureMode;
using Services::Voice::SessionState;
using Services::Voice::Snapshot;

namespace {

// Enough to answer "what did I say a few minutes ago" without letting a long
// session turn the console into an unbounded in-memory transcript.
constexpr qsizetype kMaximumEntries = 200;

} // namespace

void VoiceConsoleModel::recordDelivery(const Snapshot &snapshot)
{
    // Only a settled, non-empty final text is history. A partial is live
    // capture, and an unchanged lastText is the same dictation seen again
    // through an unrelated revision.
    if (snapshot.lastText.isEmpty() || snapshot.lastText == m_recordedText) {
        return;
    }
    if (snapshot.state == SessionState::Arming
        || snapshot.state == SessionState::Listening
        || snapshot.state == SessionState::Transcribing) {
        return;
    }
    m_recordedText = snapshot.lastText;
    m_history.prepend(HistoryEntry{
        .id = m_nextEntryId++,
        .text = snapshot.lastText,
        .routeLabel = routeLabel(),
        .timeText = QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
        .command = snapshot.mode == CaptureMode::Command,
    });
    while (m_history.size() > kMaximumEntries) {
        m_history.removeLast();
    }
    Q_EMIT historyChanged();
}

QVariantList VoiceConsoleModel::historyRows() const
{
    const QString needle = m_filter.trimmed();
    QVariantList rows;
    rows.reserve(m_history.size());
    for (const HistoryEntry &entry : m_history) {
        if (!needle.isEmpty()
            && !entry.text.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        rows.append(QVariantMap{
            {QStringLiteral("entryId"), entry.id},
            {QStringLiteral("text"), entry.text},
            {QStringLiteral("routeLabel"), entry.routeLabel},
            {QStringLiteral("timeText"), entry.timeText},
            {QStringLiteral("command"), entry.command},
        });
    }
    return rows;
}

void VoiceConsoleModel::setFilterText(const QString &filter)
{
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    Q_EMIT historyChanged();
}

bool VoiceConsoleModel::copyHistoryEntry(const int entryId)
{
    for (const HistoryEntry &entry : m_history) {
        if (entry.id != entryId) {
            continue;
        }
        // The console's own clipboard, not the provider's. Copying something
        // the user is already looking at needs no round trip and works when
        // the provider has gone away.
        if (QClipboard *clipboard = QGuiApplication::clipboard()) {
            clipboard->setText(entry.text);
            publishFeedback(tr("Copied."), QStringLiteral("success"));
            Q_EMIT viewChanged();
            return true;
        }
        publishFeedback(tr("There is no clipboard to copy into."),
                        QStringLiteral("warning"));
        Q_EMIT viewChanged();
        return false;
    }
    return false;
}

void VoiceConsoleModel::clearHistory()
{
    if (m_history.isEmpty()) {
        return;
    }
    m_history.clear();
    // Deliberately not cleared: it is the de-duplication mark for the
    // provider's current lastText, not part of the history. Clearing it would
    // make the next unrelated revision re-record a dictation the user just
    // erased.
    Q_EMIT historyChanged();
}

} // namespace QindaQt::Apps::Voice
