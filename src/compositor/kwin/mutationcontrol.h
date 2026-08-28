// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>

namespace QindaQt::Compositor::KWinIntegration {

[[nodiscard]] bool mutationsEnabledForSession(const QByteArray &developmentControl,
                                              const QByteArray &testScenario);
[[nodiscard]] bool mutationsEnabledForCurrentSession();
[[nodiscard]] bool developmentVirtualOutputsEnabledForSession(
    const QByteArray &developmentControl,
    const QByteArray &testScenario,
    const QByteArray &outputBackend);
[[nodiscard]] bool developmentVirtualOutputsEnabledForCurrentSession();

} // namespace QindaQt::Compositor::KWinIntegration
