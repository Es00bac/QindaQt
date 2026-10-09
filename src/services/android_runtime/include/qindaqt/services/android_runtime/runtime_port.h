// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/managed_applications.h"

#include <QObject>
#include <optional>

namespace QindaQt::Services::AndroidRuntime {
using QindaQt::ShellLauncher::ManagedApplicationBinding;
using QindaQt::ShellLauncher::ManagedApplicationRegistration;

enum class RuntimeState { Unavailable, NeedsSetup, Ready, Starting, NeedsRepair };
enum class ApplicationState { NotMaterialized, Ready, Starting, Running, Stopping, NeedsRepair, Unavailable };
enum class LaunchPhase { Accepted, Completed, Failed, OutcomeUnknown };

struct ApplicationStatus {
    ManagedApplicationBinding binding;
    ApplicationState state = ApplicationState::Unavailable;
    std::optional<quint32> windowCount = std::nullopt;
    bool launchAvailable = false;
    QString message;
    friend bool operator==(const ApplicationStatus &, const ApplicationStatus &) = default;
};

// Owning copied presentation facts, never a wire/persistence schema. The port
// authenticates its source and fences replaced owners before publishing them.
// Ready means launch admission, not a running Android process or visible app.
struct RuntimeSnapshot {
    QString attachmentId;
    quint64 revision = 0;
    RuntimeState state = RuntimeState::Unavailable;
    QString message;
    QVector<ManagedApplicationRegistration> registrations;
    QVector<ApplicationStatus> applications;
    friend bool operator==(const RuntimeSnapshot &, const RuntimeSnapshot &) = default;
};

struct LaunchRequest {
    QString operationId;
    QString attachmentId;
    quint64 snapshotRevision = 0;
    ManagedApplicationBinding application;
};
struct LaunchSubmission {
    bool accepted = false;
    QString message;
};
struct LaunchResult {
    QString operationId;
    QString attachmentId;
    ManagedApplicationBinding application;
    LaunchPhase phase = LaunchPhase::OutcomeUnknown;
    QString message;
};

[[nodiscard]] bool validateRuntimeSnapshot(const RuntimeSnapshot &snapshot,
                                           QString *error = nullptr);
[[nodiscard]] bool validRuntimeMessage(const QString &message);

// Borrowed nonblocking GUI-thread port; it must be created on the controller's
// thread. The composed port must obtain authenticated package/materialization
// facts and current revision admission from its qualified bootstrap/collector.
// A descriptor adapter checks transport/kernel lifetime only; it does not
// collect or attest guest facts (docs/wiki/adr/0372-bind-private-android-producers-to-kernel-lifetimes.md).
// Implementations return copied values, never
// shell argv or private paths. Destruction/loss withdraws all launch authority.
// requestLaunch rechecks the exact attachment/revision/recipe before dispatch,
// emits bounded terminal results and never automatically replays a mutation.
// The UI's 30s deadline reports uncertainty, not cancellation or Running.
class RuntimePort : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] virtual RuntimeSnapshot snapshot() const = 0;
    virtual void requestRefresh() = 0;
    [[nodiscard]] virtual LaunchSubmission requestLaunch(const LaunchRequest &request) = 0;
Q_SIGNALS:
    void snapshotChanged();
    void launchFinished(const QindaQt::Services::AndroidRuntime::LaunchResult &result);
};
} // namespace QindaQt::Services::AndroidRuntime
Q_DECLARE_METATYPE(QindaQt::Services::AndroidRuntime::LaunchResult)
Q_DECLARE_METATYPE(QindaQt::Services::AndroidRuntime::LaunchRequest)
