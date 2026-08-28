// SPDX-License-Identifier: GPL-3.0-or-later
#include "mutationcontrol.h"

#include <QtGlobal>

namespace QindaQt::Compositor::KWinIntegration {

bool mutationsEnabledForSession(const QByteArray &developmentControl,
                                const QByteArray &testScenario)
{
    // AGENT-GUARD: A marker from either side alone is insufficient. External
    // mutations are reserved for launcher-created, explicit test scenarios.
    return developmentControl == QByteArrayLiteral("1") && !testScenario.isEmpty();
}

bool mutationsEnabledForCurrentSession()
{
    return mutationsEnabledForSession(qgetenv("QINDAQT_DEVELOPMENT_CONTROL"),
                                      qgetenv("QINDAQT_TEST_SCENARIO"));
}

bool developmentVirtualOutputsEnabledForSession(
    const QByteArray &developmentControl,
    const QByteArray &testScenario,
    const QByteArray &outputBackend)
{
    // AGENT-GUARD: The public OutputBackend ABI has no capability query and
    // some non-virtual implementations block or cannot undo creation. Only
    // the launcher-proven virtual backend may receive D0 test mutations.
    return mutationsEnabledForSession(developmentControl, testScenario)
        && outputBackend == QByteArrayLiteral("virtual");
}

bool developmentVirtualOutputsEnabledForCurrentSession()
{
    return developmentVirtualOutputsEnabledForSession(
        qgetenv("QINDAQT_DEVELOPMENT_CONTROL"),
        qgetenv("QINDAQT_TEST_SCENARIO"),
        qgetenv("QINDAQT_DEVELOPMENT_OUTPUT_BACKEND"));
}

} // namespace QindaQt::Compositor::KWinIntegration
