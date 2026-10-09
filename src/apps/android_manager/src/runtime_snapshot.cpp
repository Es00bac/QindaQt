// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/android_manager/runtime_port.h"
#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QHash>
#include <QUuid>

namespace QindaQt::AndroidManager {
using namespace QindaQt::ShellLauncher;

bool validRuntimeMessage(const QString &message)
{
    if (message.size() > 1024)
        return false;
    for (qsizetype i = 0; i < message.size(); ++i) {
        const QChar c = message.at(i);
        if (c.category() == QChar::Other_Control)
            return false;
        if (c.isHighSurrogate()) {
            if (++i >= message.size() || !message.at(i).isLowSurrogate())
                return false;
        } else if (c.isLowSurrogate()) {
            return false;
        }
    }
    return message.toUtf8().size() <= 1024;
}

bool validateRuntimeSnapshot(const RuntimeSnapshot &snapshot, QString *error)
{
    if (error) error->clear();
    const auto refuse = [error](const QString &reason) {
        if (error) *error = reason;
        return false;
    };
    switch (snapshot.state) {
    case RuntimeState::Unavailable:
    case RuntimeState::NeedsSetup:
    case RuntimeState::Ready:
    case RuntimeState::Starting:
    case RuntimeState::NeedsRepair: break;
    default: return refuse(QStringLiteral("Android status is invalid"));
    }
    if (!validRuntimeMessage(snapshot.message))
        return refuse(QStringLiteral("Android status message is invalid"));
    if (snapshot.state == RuntimeState::Unavailable && snapshot.attachmentId.isEmpty()) {
        return snapshot.revision == 0 && snapshot.registrations.isEmpty()
            && snapshot.applications.isEmpty()
            ? true : refuse(QStringLiteral("Unavailable Android status retains stale applications"));
    }
    if (snapshot.attachmentId.size() != 36)
        return refuse(QStringLiteral("Android status attachment is invalid"));
    const QUuid id = QUuid::fromString(snapshot.attachmentId);
    if (id.isNull() || id.toString(QUuid::WithoutBraces) != snapshot.attachmentId
        || snapshot.revision == 0)
        return refuse(QStringLiteral("Android status attachment is invalid"));
    if (!validateManagedApplications(snapshot.registrations)
        || snapshot.applications.size() != snapshot.registrations.size())
        return refuse(QStringLiteral("Android application inventory is invalid"));
    QHash<QString, ManagedApplicationBinding> registrations;
    for (const auto &registration : snapshot.registrations) {
        if (registration.binding.origin != ApplicationOrigin::Android)
            return refuse(QStringLiteral("Android inventory contains a different platform"));
        registrations.insert(managedApplicationEntryId(registration.binding), registration.binding);
    }
    quint64 windows = 0;
    for (const auto &application : snapshot.applications) {
        const QString entryId = managedApplicationEntryId(application.binding);
        if (!registrations.contains(entryId) || registrations.take(entryId) != application.binding
            || !validRuntimeMessage(application.message))
            return refuse(QStringLiteral("Android application status does not match its registration"));
        switch (application.state) {
        case ApplicationState::Ready:
        case ApplicationState::Running: break;
        case ApplicationState::NotMaterialized:
        case ApplicationState::Starting:
        case ApplicationState::Stopping:
        case ApplicationState::NeedsRepair:
        case ApplicationState::Unavailable:
            if (application.launchAvailable)
                return refuse(QStringLiteral("Android launch availability contradicts application status"));
            break;
        default: return refuse(QStringLiteral("Android application state is invalid"));
        }
        if (application.windowCount) windows += *application.windowCount;
        if (windows > quint64(Bounds::maxVisibleEntries))
            return refuse(QStringLiteral("Android window-count ceiling was exceeded"));
    }
    return true;
}
} // namespace QindaQt::AndroidManager
