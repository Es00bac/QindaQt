// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/windows_runner/launch_ports.h>

namespace QindaQt::WindowsRunner {

// Synchronous coordinator on one caller-owned worker thread. All three borrowed
// ports outlive it and must not reenter launch(). Profile/payload work stays in
// the ports; this object owns no session service, process scope or GUI state.
// Each call is an explicit caller operation; a broker must deduplicate its own
// operation IDs before calling it. This boundary never automatically retries.
class WindowsApplicationRunner final {
public:
    WindowsApplicationRunner(WindowsRegistrationPort &registrations,
                             WindowsPayloadPort &payloads,
                             WindowsLaunchPort &launcher);
    [[nodiscard]] WindowsLaunchResult launch(
        const ManagedWindowsBinding &binding,
        const QProcessEnvironment &sessionEnvironment = {});

private:
    WindowsRegistrationPort &m_registrations;
    WindowsPayloadPort &m_payloads;
    WindowsLaunchPort &m_launcher;
};

} // namespace QindaQt::WindowsRunner
