// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/android_manager/application_controller.h"
#include <QTest>

namespace QindaQt::AndroidManager::TestSupport {
// Synthetic port/rows only. They are not stock package receipts or runtime
// authority and never execute an Android application or inspect the host.
inline RuntimeSnapshot readySnapshot()
{
    RuntimeSnapshot value;
    value.attachmentId = QStringLiteral("01234567-1234-4234-8234-123456789abc");
    value.revision = 1;
    value.state = RuntimeState::Ready;
    for (int i = 0; i < 2; ++i) {
        QindaQt::ShellLauncher::ManagedApplicationRegistration registration;
        registration.binding = {QindaQt::ShellLauncher::ApplicationOrigin::Android,
            QStringLiteral("12345678-1234-4234-8234-123456789ab%1").arg(i),
            1, QString(64, QLatin1Char('a'))};
        registration.name = i == 0 ? QStringLiteral("Synthetic Calculator") : QStringLiteral("Synthetic Clock");
        registration.categories = {QStringLiteral("Utility")};
        registration.keywords = {i == 0 ? QStringLiteral("arithmetic") : QStringLiteral("time")};
        value.registrations.append(registration);
        value.applications.append({registration.binding, ApplicationState::Ready, std::nullopt, true, {}});
    }
    return value;
}
inline QString entryId(const RuntimeSnapshot &snapshot, qsizetype index = 0)
{
    return QindaQt::ShellLauncher::managedApplicationEntryId(snapshot.registrations.at(index).binding);
}
class FakePort final : public RuntimePort {
public:
    RuntimeSnapshot current = readySnapshot();
    QVector<LaunchRequest> requests;
    int refreshes = 0;
    bool accept = true;
    bool finishInline = false;
    RuntimeSnapshot snapshot() const override { return current; }
    void requestRefresh() override { ++refreshes; }
    LaunchSubmission requestLaunch(const LaunchRequest &request) override {
        requests.append(request);
        if (finishInline) result(request, LaunchPhase::Completed);
        return {accept, accept ? QString() : QStringLiteral("Synthetic refusal")};
    }
    void publish(RuntimeSnapshot snapshot) { current = std::move(snapshot); Q_EMIT snapshotChanged(); }
    void result(const LaunchRequest &request, LaunchPhase phase, const QString &message = {}) {
        Q_EMIT launchFinished({request.operationId, request.attachmentId, request.application, phase, message});
    }
};
} // namespace QindaQt::AndroidManager::TestSupport
