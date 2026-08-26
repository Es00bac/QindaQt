// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Test {

class CompositorProbeClient;
struct ProbeWindowTitles;

struct HybridPointerWorkflowResult final
{
    QJsonObject evidence;
    QJsonObject finalHybridDiagnostics;
};

// Keeps one dotool uinput producer alive while driving the process-local Hybrid
// input path. KWin virtual seats that reject host uinput use the disclosed,
// development-gated InputDevice fallback; neither path calls topology APIs.
// Window state and diagnostics remain on the public D-Bus boundary.
[[nodiscard]] std::optional<HybridPointerWorkflowResult>
exerciseHybridPointerWorkflow(CompositorProbeClient &client,
                              const ProbeWindowTitles &titles,
                              const QString &dotoolPath,
                              const std::function<void(const QString &)> &activateProbe,
                              const std::function<void(const QString &)> &showPopupForProbe,
                              const std::function<QString(const QString &)> &showDialogForProbe,
                              QString *error);

} // namespace QindaQt::Test
