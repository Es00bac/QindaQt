// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

// Deterministic, obviously synthetic clipboard fixtures. AGENT-GUARD: every
// payload here must remain a recognizable "fixture" token or synthetic byte
// pattern; real user clipboard content must never enter this repository
// through tests, logs, or board messages.
using namespace QindaQt::Services::ClipboardModel;

namespace ClipboardTest {

inline constexpr quint32 kGen = 1; // initial model generation

inline QString textFormat()
{
    return QStringLiteral("text/plain");
}

inline QString htmlFormat()
{
    return QStringLiteral("text/html");
}

inline QString uriFormat()
{
    return QStringLiteral("text/uri-list");
}

inline QString pngFormat()
{
    return QStringLiteral("image/png");
}

inline QString sensitiveMarker()
{
    return QStringLiteral("x-kde-passwordmanagerhint");
}

inline QString qindaqtSecret()
{
    return QStringLiteral("application/x-qindaqt-secret");
}

inline QString oneTimeMarker()
{
    return QStringLiteral("x-qindaqt-one-time");
}

inline QString unknownMediaType()
{
    return QStringLiteral("application/x-unknown-vendor-blob");
}

inline ClipboardValue textValue(const QString &text)
{
    ClipboardValue value;
    value.formats.append({ textFormat(), text.toUtf8() });
    return value;
}

inline ClipboardValue fixtureAlpha()
{
    return textValue(QStringLiteral("fixture alpha payload"));
}

inline ClipboardValue fixtureBeta()
{
    ClipboardValue value;
    value.formats.append({ textFormat(), QByteArrayLiteral("fixture beta payload") });
    value.formats.append(
        { htmlFormat(), QByteArrayLiteral("<p>fixture beta</p>") });
    return value;
}

// Synthetic PNG-signature bytes with a fixture marker; not a decodable image
// and not required to be.
inline ClipboardValue fixturePngLike()
{
    ClipboardValue value;
    QByteArray payload;
    payload.append('\x89');
    payload.append(QByteArrayLiteral("PNG\r\n\x1a\nfixture-synthetic-bytes"));
    value.formats.append({ pngFormat(), payload });
    return value;
}

inline ClipboardValue valueWithMedia(const QString &mediaType, const QString &text)
{
    ClipboardValue value;
    value.formats.append({ mediaType, text.toUtf8() });
    return value;
}

// Shared model helpers for history suites. AGENT-NOTE: these live beside the
// fixtures so every clipboard suite constructs models the same fail-closed
// way; the enabled helper is the only sanctioned shortcut past the disabled
// default, and every test still exercises gating explicitly.
inline ClipboardHistoryModel enabledModel(const HistoryLimits &limits = HistoryLimits {})
{
    ClipboardHistoryModel model(limits);
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);
    return model;
}

inline HistoryLimits limitsOf(int entries, qint64 totalBytes)
{
    HistoryLimits limits;
    limits.maxEntries = entries;
    // Narrowing entries also bounds the pin ceiling, and narrowing the total
    // must narrow the per-item bound with it: limits must stay valid.
    limits.maxPinnedEntries = qMin(limits.maxPinnedEntries, entries);
    limits.maxTotalPayloadBytes = totalBytes;
    limits.maxItemPayloadBytes = qMin(limits.maxItemPayloadBytes, totalBytes);
    return limits;
}

inline QList<QString> previewOrder(const HistorySnapshot &snapshot)
{
    QList<QString> previews;
    previews.reserve(snapshot.entries.size());
    for (const ClipboardEntryDescriptor &entry : snapshot.entries) {
        previews.append(entry.preview);
    }
    return previews;
}

} // namespace ClipboardTest
