// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/windows_runner/windows_runner.h>

#include <qindaqt/windows_runner/launch_plan.h>

#include <QUuid>

namespace QindaQt::WindowsRunner {
namespace {

WindowsLaunchResult unavailable(LaunchFailure failure, const QString &entryId)
{
    QString message;
    switch (failure) {
    case LaunchFailure::InvalidBinding:
        message = QStringLiteral("This Windows application registration is invalid."); break;
    case LaunchFailure::MissingRegistration:
        message = QStringLiteral("This Windows application is no longer registered."); break;
    case LaunchFailure::RegistrationChanged:
        message = QStringLiteral("This Windows application changed. Refresh Applications and try again."); break;
    case LaunchFailure::ProfileNotReady:
        message = QStringLiteral("This Windows application's profile needs repair before launch."); break;
    case LaunchFailure::InvalidRecipe:
        message = QStringLiteral("This Windows application's installed recipe is unavailable."); break;
    case LaunchFailure::PayloadChanged:
        message = QStringLiteral("This Windows application's installed files changed. Repair its package before launch."); break;
    case LaunchFailure::DispatchRefused:
        message = QStringLiteral("This Windows application could not start."); break;
    default:
        message = QStringLiteral("This Windows application is unavailable."); break;
    }
    return {LaunchState::Unavailable, failure, entryId, {}, message};
}

} // namespace

WindowsApplicationRunner::WindowsApplicationRunner(
    WindowsRegistrationPort &registrations, WindowsPayloadPort &payloads,
    WindowsLaunchPort &launcher)
    : m_registrations(registrations), m_payloads(payloads), m_launcher(launcher)
{
}

WindowsLaunchResult WindowsApplicationRunner::launch(
    const ManagedWindowsBinding &binding,
    const QProcessEnvironment &sessionEnvironment)
{
    const QString entryId = windowsCatalogEntryId(binding);
    if (entryId.isEmpty()) {
        return unavailable(LaunchFailure::InvalidBinding, {});
    }
    const auto registration = m_registrations.resolve(binding.registrationId);
    if (!registration) {
        return unavailable(LaunchFailure::MissingRegistration, entryId);
    }
    auto preparation = prepareWindowsLaunch(binding, *registration, sessionEnvironment);
    if (!preparation.plan) {
        return unavailable(preparation.failure, entryId);
    }
    auto lease = m_payloads.acquire(*registration);
    if (!lease || lease->verifiedPayloads() != preparation.plan->payloads
        || !lease->stillCurrent()) {
        return unavailable(LaunchFailure::PayloadChanged, entryId);
    }
    // AGENT-GUARD: hashing/materialization inspection can overlap an update or
    // removal. Re-resolve the entire copied recipe, not only its UUID/revision,
    // before dispatch. A changed payload with a reused revision still refuses.
    const auto current = m_registrations.resolve(binding.registrationId);
    if (!current || *current != *registration) {
        return unavailable(LaunchFailure::RegistrationChanged, entryId);
    }
    if (!lease->stillCurrent()) {
        return unavailable(LaunchFailure::PayloadChanged, entryId);
    }
    WindowsLaunchPlan &plan = *preparation.plan;
    plan.launchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    switch (m_launcher.submit(plan, *lease)) {
    case LaunchDispatch::Accepted:
        return {LaunchState::Starting, LaunchFailure::None, entryId,
                plan.launchId, QStringLiteral("Starting Windows application.")};
    case LaunchDispatch::OutcomeUnknown:
        return {LaunchState::OutcomeUnknown, LaunchFailure::DispatchUncertain,
                entryId, plan.launchId,
                QStringLiteral("The Windows application launch outcome is unknown.")};
    case LaunchDispatch::Refused:
        return unavailable(LaunchFailure::DispatchRefused, entryId);
    }
    return {LaunchState::OutcomeUnknown, LaunchFailure::DispatchUncertain,
            entryId, plan.launchId,
            QStringLiteral("The Windows application launch outcome is unknown.")};
}

} // namespace QindaQt::WindowsRunner
