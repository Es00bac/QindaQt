// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/windows_runner/launch_plan.h>
#include <qindaqt/windows_runner/windows_runner.h>

#include <functional>

namespace WindowsRunnerTests {
using namespace QindaQt::WindowsRunner;

inline RegisteredWindowsApplication application()
{
    RegisteredWindowsApplication app;
    app.binding = {QStringLiteral("427e0914-558e-4d1e-9b34-001933ef126f"),
                   3, QString(64, QLatin1Char('a'))};
    app.packageCpv = QStringLiteral("app-emulation/wine-proton-11.0.2");
    app.packageRepository = QStringLiteral("qindaqt");
    app.packageSlot = QStringLiteral("11");
    app.profileId = QStringLiteral("03783c78-8cf3-4cdd-a403-03d7feeb5347");
    app.profileHome = QStringLiteral("/private/profiles/notepad/home");
    app.prefixPath = QStringLiteral("/private/profiles/notepad/prefix");
    app.runtime.launcher = {QStringLiteral("/usr/lib/wine-proton-11.0.2/bin/wine"),
                            QByteArray(32, 'w'), 14280};
    app.application = {QStringLiteral("/usr/lib/wine-proton-11.0.2/wine/x86_64-windows/notepad.exe"),
                       QByteArray(32, 'n'), 280173};
    app.materializationReady = true;
    return app;
}

struct Registrations final : WindowsRegistrationPort {
    std::optional<RegisteredWindowsApplication> current = application();
    std::function<void(int)> onResolve;
    int reads = 0;
    std::optional<RegisteredWindowsApplication> resolve(const QString &id) override
    {
        ++reads;
        if (onResolve) { onResolve(reads); }
        return current && current->binding.registrationId == id ? current : std::nullopt;
    }
};

struct Lease final : WindowsPayloadLease {
    QVector<PayloadPin> pins;
    std::function<bool(int)> freshness;
    mutable int checks = 0;
    const QVector<PayloadPin> &verifiedPayloads() const override { return pins; }
    bool stillCurrent() const override
    {
        ++checks;
        return !freshness || freshness(checks);
    }
};

struct Payloads final : WindowsPayloadPort {
    std::function<void(Lease &)> amend;
    std::function<void()> onAcquire;
    bool available = true;
    int acquisitions = 0;
    std::unique_ptr<WindowsPayloadLease> acquire(const RegisteredWindowsApplication &app) override
    {
        ++acquisitions;
        if (onAcquire) { onAcquire(); }
        if (!available) { return {}; }
        auto lease = std::make_unique<Lease>();
        lease->pins = prepareWindowsLaunch(app.binding, app, {}).plan->payloads;
        if (amend) { amend(*lease); }
        return lease;
    }
};

struct Launcher final : WindowsLaunchPort {
    LaunchDispatch outcome = LaunchDispatch::Accepted;
    QVector<WindowsLaunchPlan> submissions;
    LaunchDispatch submit(const WindowsLaunchPlan &plan, WindowsPayloadLease &lease) override
    {
        if (!lease.stillCurrent()) { return LaunchDispatch::Refused; }
        submissions.append(plan);
        return outcome;
    }
};
} // namespace WindowsRunnerTests
