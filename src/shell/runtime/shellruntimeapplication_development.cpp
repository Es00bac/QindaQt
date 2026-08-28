// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "notificationquietingsettingsbridge.h"
#include "notificationwindowcontroller.h"
#include "shelldevelopmentevidence.h"

namespace QindaQt::Shell {

bool ShellRuntimeApplication::startDevelopmentEvidence(
    const RuntimeOptions &options, QString *error)
{
    if (qEnvironmentVariable("QINDAQT_DEVELOPMENT_CONTROL")
        != QLatin1String("1")) {
        return true;
    }
    if (!m_notificationWindows) {
        *error = QStringLiteral(
            "development evidence requires supervised notification authority");
        return false;
    }
    // AGENT-CONTRACT: The evidence service is the nested harness's shell-ready
    // signal. Publish it only after production shortcuts and initial surfaces
    // are reconciled so PID authentication cannot race unfinished startup.
    m_shellDevelopmentEvidence = std::make_unique<ShellDevelopmentEvidence>(
        *m_notificationPresentation, m_quietingSettingsBridge->controller(),
        *m_notificationPrivacyPolicy, *m_notificationWindows);
    return m_shellDevelopmentEvidence->start(
        *options.compositorProcessId,
        options.developmentEvidencePredecessorProcessId.value_or(0), error);
}

} // namespace QindaQt::Shell
