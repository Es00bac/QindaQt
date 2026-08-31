// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

class QDBusConnection;
class QDBusConnectionInterface;

namespace QindaQt::Test {

enum class DesktopNotificationShellPhase {
    ClosedHidden,
    OpenVisible,
};

enum class DesktopNotificationShellDisposition {
    Ready,
    Pending,
    Invalid,
};

struct DesktopNotificationShellObservation final {
    QString owner;
    QString ownerAfterSnapshot;
    qint64 serviceProcessId = 0;
    bool snapshotReplyValid = false;
    QJsonObject snapshot;
    QString replyError;
    QString replyErrorName;
    bool serviceOwnerReplyValid = true;
    QString serviceOwnerReplyError;
    QString serviceOwnerReplyErrorName;
};

struct DesktopNotificationShellExpectation final {
    qint64 dockProcessId = 0;
    QString outputName;
    QString requiredUniqueOwner;
    DesktopNotificationShellPhase phase =
        DesktopNotificationShellPhase::ClosedHidden;
    quint64 centerOpenedCountBefore = 0;
};

struct DesktopNotificationShellCheck final {
    DesktopNotificationShellDisposition disposition =
        DesktopNotificationShellDisposition::Invalid;
    QString code;
    QString message;
    QJsonObject evidence;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] QJsonObject document() const;
};

struct DesktopNotificationShellExpectationCheck final {
    DesktopNotificationShellDisposition disposition =
        DesktopNotificationShellDisposition::Invalid;
    std::optional<DesktopNotificationShellExpectation> expectation;
    QString code;
    QString message;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] QJsonObject document() const;
};

[[nodiscard]] DesktopNotificationShellCheck validateDesktopNotificationShell(
    const DesktopNotificationShellObservation &observation,
    const DesktopNotificationShellExpectation &expectation);

// One D-Bus sample only. The caller owns every retry budget and may invoke this
// again from an existing outer state-observation loop.
[[nodiscard]] DesktopNotificationShellCheck sampleDesktopNotificationShell(
    const QDBusConnection &connection, QDBusConnectionInterface &bus,
    const DesktopNotificationShellExpectation &expectation);

[[nodiscard]] DesktopNotificationShellExpectationCheck
desktopNotificationShellExpectation(
    const QJsonObject &developmentShellSurfaces, const QJsonObject &outputs,
    const QString &requestedOutput, DesktopNotificationShellPhase phase,
    quint64 centerOpenedCountBefore, QString *error);

} // namespace QindaQt::Test
