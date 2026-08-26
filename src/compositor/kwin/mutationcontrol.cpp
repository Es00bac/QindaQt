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

} // namespace QindaQt::Compositor::KWinIntegration
