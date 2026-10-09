// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/windows_runner/launch_plan.h>

#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace QindaQt::WindowsRunner {
namespace {

bool cleanText(const QString &value, qsizetype maximum, bool allowEmpty = false)
{
    if ((!allowEmpty && value.isEmpty()) || value.toUtf8().size() > maximum
        || QString::fromUtf8(value.toUtf8()) != value) {
        return false;
    }
    for (const QChar ch : value) {
        if (ch.category() == QChar::Other_Control) {
            return false;
        }
    }
    return true;
}

bool uuid(const QString &value)
{
    const QUuid parsed(value);
    return !parsed.isNull() && parsed.toString(QUuid::WithoutBraces) == value;
}

bool digest(const QString &value)
{
    if (value.size() != 64) {
        return false;
    }
    for (const QChar ch : value) {
        if (!((ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
              || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')))) {
            return false;
        }
    }
    return true;
}

bool absolutePath(const QString &value)
{
    return cleanText(value, 4096) && QDir::isAbsolutePath(value)
        && value != QStringLiteral("/") && QDir::cleanPath(value) == value;
}

bool payload(const PayloadPin &pin)
{
    return absolutePath(pin.path) && pin.sha256.size() == 32 && pin.bytes > 0;
}

bool packageToken(const QString &value)
{
    if (!cleanText(value, 256)) {
        return false;
    }
    for (const QChar ch : value) {
        if (!(ch.isLetterOrNumber() || QStringLiteral("+_.-/:").contains(ch))) {
            return false;
        }
    }
    return true;
}

QProcessEnvironment launchEnvironment(const RegisteredWindowsApplication &app,
                                     const QProcessEnvironment &session)
{
    // AGENT-GUARD: copy an allowlist into an empty environment. Removing a few
    // known Wine overrides from the host would leave loader/Python steering and
    // future runtime override variables able to bypass the package pins.
    QProcessEnvironment environment;
    const QStringList fixedKeys{
        QStringLiteral("LANG"), QStringLiteral("LC_ALL"),
        QStringLiteral("DISPLAY"), QStringLiteral("WAYLAND_DISPLAY"),
        QStringLiteral("XDG_RUNTIME_DIR"), QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
        QStringLiteral("PULSE_SERVER"), QStringLiteral("PIPEWIRE_REMOTE")};
    for (const QString &key : session.keys()) {
        if ((fixedKeys.contains(key) || key.startsWith(QStringLiteral("LC_")))
            && cleanText(session.value(key), 4096)) {
            environment.insert(key, session.value(key));
        }
    }
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));
    environment.insert(QStringLiteral("HOME"), app.profileHome);
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), app.profileHome + QStringLiteral("/config"));
    environment.insert(QStringLiteral("XDG_DATA_HOME"), app.profileHome + QStringLiteral("/data"));
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), app.profileHome + QStringLiteral("/cache"));
    environment.insert(QStringLiteral("XDG_STATE_HOME"), app.profileHome + QStringLiteral("/state"));
    environment.insert(QStringLiteral("WINEPREFIX"), app.prefixPath);
    if (app.runtime.backend == RuntimeBackend::ProtonUmu) {
        environment.insert(QStringLiteral("PROTONPATH"), app.runtime.protonDirectory);
        environment.insert(QStringLiteral("GAMEID"), QStringLiteral("umu-0"));
        environment.insert(QStringLiteral("STORE"), QStringLiteral("none"));
        environment.insert(QStringLiteral("UMU_RUNTIME_UPDATE"), QStringLiteral("0"));
    }
    return environment;
}

} // namespace

bool isValidWindowsBinding(const ManagedWindowsBinding &binding)
{
    return uuid(binding.registrationId) && binding.registrationRevision > 0
        && digest(binding.recipeDigest);
}

QString windowsCatalogEntryId(const ManagedWindowsBinding &binding)
{
    return isValidWindowsBinding(binding)
        ? QStringLiteral("qindaqt.managed.windows.") + binding.registrationId : QString{};
}

WindowsLaunchPreparation prepareWindowsLaunch(
    const ManagedWindowsBinding &binding,
    const RegisteredWindowsApplication &app,
    const QProcessEnvironment &session)
{
    if (!isValidWindowsBinding(binding)) {
        return {std::nullopt, LaunchFailure::InvalidBinding};
    }
    if (app.binding != binding) {
        return {std::nullopt, LaunchFailure::RegistrationChanged};
    }
    if (!app.materializationReady) {
        return {std::nullopt, LaunchFailure::ProfileNotReady};
    }
    if (!packageToken(app.packageCpv) || !app.packageCpv.contains(QLatin1Char('/'))
        || !packageToken(app.packageRepository) || !packageToken(app.packageSlot)
        || !uuid(app.profileId) || !absolutePath(app.profileHome)
        || !absolutePath(app.prefixPath) || !payload(app.application)
        || !payload(app.runtime.launcher) || app.arguments.size() > 64) {
        return {std::nullopt, LaunchFailure::InvalidRecipe};
    }
    for (const QString &argument : app.arguments) {
        if (!cleanText(argument, 4096, true)) {
            return {std::nullopt, LaunchFailure::InvalidRecipe};
        }
    }
    WindowsLaunchPlan plan;
    plan.binding = binding;
    plan.catalogEntryId = windowsCatalogEntryId(binding);
    plan.program = app.runtime.launcher.path;
    plan.arguments = QStringList{app.application.path};
    plan.arguments.append(app.arguments);
    plan.workingDirectory = QFileInfo(app.application.path).absolutePath();
    plan.payloads = {app.runtime.launcher, app.application};
    switch (app.runtime.backend) {
    case RuntimeBackend::Wine:
        if (!app.runtime.protonDirectory.isEmpty()
            || app.runtime.protonEntrypoint != PayloadPin{}
            || app.runtime.steamRuntimeReceipt != PayloadPin{}) {
            return {std::nullopt, LaunchFailure::InvalidRecipe};
        }
        break;
    case RuntimeBackend::ProtonUmu:
        if (!absolutePath(app.runtime.protonDirectory)
            || !payload(app.runtime.protonEntrypoint)
            || app.runtime.protonEntrypoint.path
                != app.runtime.protonDirectory + QStringLiteral("/proton")
            || !payload(app.runtime.steamRuntimeReceipt)) {
            return {std::nullopt, LaunchFailure::InvalidRecipe};
        }
        plan.payloads.append(app.runtime.protonEntrypoint);
        plan.payloads.append(app.runtime.steamRuntimeReceipt);
        break;
    default:
        return {std::nullopt, LaunchFailure::InvalidRecipe};
    }
    for (qsizetype i = 0; i < plan.payloads.size(); ++i) {
        for (qsizetype j = i + 1; j < plan.payloads.size(); ++j) {
            if (plan.payloads[i].path == plan.payloads[j].path) {
                return {std::nullopt, LaunchFailure::InvalidRecipe};
            }
        }
    }
    plan.environment = launchEnvironment(app, session);
    return {plan, LaunchFailure::None};
}

} // namespace QindaQt::WindowsRunner
