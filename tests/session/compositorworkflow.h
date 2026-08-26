// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace QindaQt::Test {

enum class CompositorWorkflowMode
{
    InventoryOnly,
    DevelopmentMutations,
    ProductionReadOnly,
};

struct CompositorWorkflowResult final
{
    bool serviceAvailable = false;
    QString kwinAbi;
    QString controlMode;
    bool mutationsEnabled = false;
    bool inputObserverActive = false;
    bool inputConsumesEvents = false;
    QJsonArray inputDevices;
    bool workflowPassed = false;
    QString failure;
    QJsonObject evidence;
    QJsonArray outputs;
};

// AGENT-CONTRACT: This probe exercises the public D-Bus boundary against three
// real Wayland windows. It must not reach into KWin or compositor internals;
// otherwise a passing nested test would not prove the shipped control surface.
[[nodiscard]] CompositorWorkflowResult exerciseCompositorWorkflow(const QString &primaryTitle,
                                                                  const QString &secondaryTitle,
                                                                  const QString &pageTitle,
                                                                  CompositorWorkflowMode mode);

} // namespace QindaQt::Test
