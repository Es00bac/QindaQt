// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace QindaQt::Services::NotificationPresentation {

struct PresentationAction final {
    QString key;
    QString label;

    bool operator==(const PresentationAction &) const = default;
};

struct PresentationNotification final {
    quint32 id = 0;
    QString applicationName;
    QString applicationIcon;
    QString summary;
    QString body;
    quint32 urgency = 1;
    QString desktopEntry;
    QString imagePath;
    bool resident = false;
    bool transient = false;
    qint64 createdAtMs = 0;
    std::optional<qint64> updatedAtMs;
    std::optional<qint64> expiresAtMs;
    QVector<PresentationAction> actions;

    bool operator==(const PresentationNotification &) const = default;
};

struct PresentationSnapshot final {
    QString epoch;
    quint64 revision = 0;
    QVector<PresentationNotification> notifications;

    bool operator==(const PresentationSnapshot &) const = default;
};

struct SnapshotDecodeResult final {
    std::optional<PresentationSnapshot> snapshot;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return snapshot.has_value(); }
};

class PresentationSnapshotCodec final {
public:
    // AGENT-CONTRACT: encode accepts trusted host values. Every consumer,
    // including the host before publication, must accept only decode() output.
    [[nodiscard]] static QVariantMap encode(const PresentationSnapshot &snapshot);
    [[nodiscard]] static SnapshotDecodeResult decode(const QVariantMap &wire);
};

} // namespace QindaQt::Services::NotificationPresentation
