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
// forceDevelopmentInput bypasses dotool selection entirely (root-authorized,
// see HybridPointerGrouping::forceDevelopmentInput and
// docs/wiki/development/testing-harness.md) for hosts where the dotool
// binary itself is absent. Leaving it false preserves ordinary dotool-first
// selection for every existing caller.
[[nodiscard]] std::optional<HybridPointerWorkflowResult>
exerciseHybridPointerWorkflow(CompositorProbeClient &client,
                              const ProbeWindowTitles &titles,
                              const QString &dotoolPath,
                              const std::function<void(const QString &)> &activateProbe,
                              const std::function<void(const QString &)> &showPopupForProbe,
                              const std::function<QString(const QString &)> &showDialogForProbe,
                              QString *error,
                              bool forceDevelopmentInput = false);

} // namespace QindaQt::Test
