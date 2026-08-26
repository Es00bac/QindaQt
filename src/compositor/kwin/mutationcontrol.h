// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>

namespace QindaQt::Compositor::KWinIntegration {

[[nodiscard]] bool mutationsEnabledForSession(const QByteArray &developmentControl,
                                              const QByteArray &testScenario);
[[nodiscard]] bool mutationsEnabledForCurrentSession();

} // namespace QindaQt::Compositor::KWinIntegration
