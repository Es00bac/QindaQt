// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/protocol.h"
namespace RuntimeFixtures {
using namespace QindaQt::Services::AndroidRuntime;
inline RuntimeSnapshot snapshot() {
    RuntimeSnapshot value;
    value.attachmentId=QStringLiteral("01234567-1234-4234-8234-123456789abc");
    value.revision=1; value.state=RuntimeState::Ready;
    ManagedApplicationRegistration registration;
    registration.binding={QindaQt::ShellLauncher::ApplicationOrigin::Android,
        QStringLiteral("12345678-1234-4234-8234-123456789abc"),1,QString(64,QLatin1Char('a'))};
    registration.name=QStringLiteral("Synthetic package only");
    value.registrations.append(registration);
    value.applications.append({registration.binding,ApplicationState::Ready,std::nullopt,true,{}});
    return value;
}
inline LaunchRequest request(const RuntimeSnapshot &value) {
    return {QStringLiteral("abcdef01-1234-4234-8234-123456789abc"),value.attachmentId,value.revision,
            value.registrations.first().binding};
}
inline QByteArray key() { return QByteArray(32,'k'); }
} // namespace RuntimeFixtures
