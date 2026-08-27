// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Services::SessionLockState {

Q_NAMESPACE

enum class LockState {
    Unknown,
    Unlocked,
    Locking,
    Locked,
};
Q_ENUM_NS(LockState)

enum class ObservedService {
    Compositor,
    FreedesktopScreenSaver,
    KdeScreenSaver,
};
Q_ENUM_NS(ObservedService)

enum class LockRequest {
    ServiceOwner,
    UnixProcessId,
    ActiveState,
};
Q_ENUM_NS(LockRequest)

// Returns the stable well-known name whose exact unique owner participates in
// the three-name authentication quorum.
[[nodiscard]] QString observedServiceName(ObservedService service);

} // namespace QindaQt::Services::SessionLockState

Q_DECLARE_METATYPE(QindaQt::Services::SessionLockState::LockState)
Q_DECLARE_METATYPE(QindaQt::Services::SessionLockState::ObservedService)
Q_DECLARE_METATYPE(QindaQt::Services::SessionLockState::LockRequest)
