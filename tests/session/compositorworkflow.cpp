// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositorworkflow.h"

#include "compositordevelopmentworkflow.h"
#include "compositorprobeclient.h"
#include "hybridpointerworkflow.h"

#include <QJsonArray>
#include <QSet>

#include <algorithm>
#include <optional>

namespace QindaQt::Test {
namespace {

QSet<QString> stringSet(const QJsonValue &value)
{
    QSet<QString> result;
    for (const auto &entry : value.toArray()) {
        if (entry.isString()) {
            result.insert(entry.toString());
        }
    }
    return result;
}

bool inspectEndpoint(CompositorProbeClient &client, CompositorWorkflowMode mode,
                     CompositorWorkflowResult *result, QString *error)
{
    const bool workflowRequired = mode != CompositorWorkflowMode::InventoryOnly;
    const auto capabilities = client.call(QStringLiteral("Capabilities"), error);
    if (!capabilities) {
        result->failure = *error;
        result->workflowPassed = !workflowRequired;
        return false;
    }
    result->serviceAvailable = true;
    result->kwinAbi = capabilities->value(QStringLiteral("kwinAbi")).toString();
    result->controlMode = capabilities->value(QStringLiteral("controlMode")).toString();
    result->mutationsEnabled = capabilities->value(QStringLiteral("mutationsEnabled")).toBool();
    result->hybridDiagnostics = capabilities->value(QStringLiteral("hybrid")).toObject();
    result->developmentInputCapabilities =
        capabilities->value(QStringLiteral("developmentInput")).toObject();
    const QSet<QString> expectedMethods{QStringLiteral("Capabilities"),
                                        QStringLiteral("Windows"),
                                        QStringLiteral("Outputs"),
                                        QStringLiteral("InputCapabilities"),
                                        QStringLiteral("Containers"),
                                        QStringLiteral("DockWindows"),
                                        QStringLiteral("InjectTestInput"),
                                        QStringLiteral("ReinitializeCompositingForTest"),
                                        QStringLiteral("ReleaseContainer"),
                                        QStringLiteral("Snapshot"),
                                        QStringLiteral("Submit")};
    const QSet<QString> expectedEvents{
        QStringLiteral("ContainerCommitted"), QStringLiteral("WindowsChanged"),
        QStringLiteral("OutputsChanged"), QStringLiteral("InputCapabilitiesChanged")};
    if (stringSet(capabilities->value(QStringLiteral("methods"))) != expectedMethods ||
        stringSet(capabilities->value(QStringLiteral("events"))) != expectedEvents) {
        result->failure = QStringLiteral("Capabilities does not match Compositor1 methods/events");
        result->workflowPassed = !workflowRequired;
        return false;
    }
    if (!result->hybridDiagnostics.value(QStringLiteral("ready")).toBool()
        || !result->hybridDiagnostics
                .value(QStringLiteral("inputFilterInstalled"))
                .toBool()
        || !result->hybridDiagnostics.value(QStringLiteral("topologyRevision")).isString()
        || !result->hybridDiagnostics.value(QStringLiteral("containerCount")).isDouble()
        || !result->hybridDiagnostics.value(QStringLiteral("chromeOverlayCount")).isDouble()
        || !result->hybridDiagnostics
                .value(QStringLiteral("visibleChromeOverlayCount"))
                .isDouble()
        || !result->hybridDiagnostics
                .value(QStringLiteral("anchoredChromeSceneItemCount"))
                .isDouble()
        || !result->hybridDiagnostics
                .value(QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                .isDouble()
        || !result->hybridDiagnostics
                .value(QStringLiteral("quarantinedContainerCount"))
                .isDouble()
        || !result->hybridDiagnostics
                .value(QStringLiteral("publishedGroupStackingCount"))
                .isDouble()
        || !result->hybridDiagnostics
                .value(QStringLiteral("lastGroupStackingFailure"))
                .isString()) {
        result->failure = QStringLiteral("Hybrid compositor runtime is not ready");
        result->workflowPassed = !workflowRequired;
        return false;
    }
    if (mode != CompositorWorkflowMode::InventoryOnly) {
        const bool developmentMode =
            mode == CompositorWorkflowMode::DevelopmentMutations
            || mode == CompositorWorkflowMode::HybridPointer;
        const auto &developmentInput = result->developmentInputCapabilities;
        const QSet<QString> expectedEventTypes{QStringLiteral("pointer-absolute"),
                                               QStringLiteral("key"),
                                               QStringLiteral("button")};
        const bool commonSchemaValid =
            developmentInput.value(QStringLiteral("schemaVersion")).toInt(-1) == 1
            && developmentInput.value(QStringLiteral("maxEvents")).toInt(-1) == 64
            && developmentInput.value(QStringLiteral("deviceId"))
                == QStringLiteral("qindaqt-development-input")
            && stringSet(developmentInput.value(QStringLiteral("eventTypes")))
                == expectedEventTypes;
        const bool contractValid = developmentMode
            ? commonSchemaValid
                && developmentInput.value(QStringLiteral("enabled")).toBool()
                && developmentInput.value(QStringLiteral("available")).toBool()
            : !developmentInput.value(QStringLiteral("enabled")).toBool(true)
                && !developmentInput.value(QStringLiteral("available")).toBool(true);
        if (!contractValid) {
            result->failure = QStringLiteral(
                "Capabilities returned an invalid development input contract");
            result->workflowPassed = false;
            return false;
        }
    }

    const auto input = client.call(QStringLiteral("InputCapabilities"), error);
    if (!input || input->value(QStringLiteral("status")) != QStringLiteral("ok") ||
        input->value(QStringLiteral("schemaVersion")).toInt() != 1 ||
        !input->value(QStringLiteral("devices")).isArray()) {
        result->failure = error->isEmpty()
                              ? QStringLiteral("InputCapabilities returned an invalid schema")
                              : *error;
        result->workflowPassed = !workflowRequired;
        return false;
    }
    result->inputObserverActive = input->value(QStringLiteral("observerActive")).toBool();
    result->inputConsumesEvents = input->value(QStringLiteral("consumesEvents")).toBool(true);
    result->inputDevices = input->value(QStringLiteral("devices")).toArray();
    if (!result->inputObserverActive || result->inputConsumesEvents) {
        result->failure = QStringLiteral("the KWin input observer is inactive or consumes events");
        result->workflowPassed = !workflowRequired;
        return false;
    }

    const auto listedOutputs = client.outputs(error);
    if (!listedOutputs) {
        result->failure = *error;
        result->workflowPassed = !workflowRequired;
        return false;
    }
    // AGENT-CONTRACT: The Python scenario runner compares this compositor-side
    // inventory with Qt's client-side modes. Fractional scale cannot be proven
    // from QScreen::devicePixelRatio(), which is integer on this Wayland path.
    result->outputs = *listedOutputs;
    return true;
}

void refreshHybridDiagnostics(CompositorProbeClient &client,
                              CompositorWorkflowResult *result)
{
    QString ignoredError;
    const auto capabilities = client.call(QStringLiteral("Capabilities"),
                                          &ignoredError);
    if (capabilities) {
        // AGENT-GUARD: A failed live workflow must report the post-failure
        // topology, not the capability snapshot captured before any gesture.
        // Otherwise an uncleared group is disguised as revision zero/count 0.
        result->hybridDiagnostics =
            capabilities->value(QStringLiteral("hybrid")).toObject();
    }
}

bool rejectedWith(const std::optional<QJsonObject> &reply, QLatin1StringView code)
{
    return reply && reply->value(QStringLiteral("status")) == QStringLiteral("rejected") &&
           reply->value(QStringLiteral("failure"))
                   .toObject()
                   .value(QStringLiteral("code"))
                   .toString() == code;
}

std::optional<WindowInventory> awaitQindaDecorations(CompositorProbeClient &client,
                                                     const ProbeWindowTitles &titles,
                                                     QString *error)
{
    const QStringList expectedTitles{titles.primary, titles.secondary, titles.page};
    auto inventory = client.awaitWindows(
        expectedTitles,
        [expectedTitles](const WindowInventory &windows) {
            return windows.size() == expectedTitles.size()
                && std::all_of(windows.cbegin(), windows.cend(), [](const ObservedWindow &window) {
                       return window.serverDecorated
                           && window.decorationClass.contains(QStringLiteral("QindaDecoration"));
                   });
        },
        error);
    if (!inventory && error->isEmpty()) {
        *error = QStringLiteral("KWin did not instantiate QindaQt decorations on every probe");
    }
    return inventory;
}

bool exerciseReadOnlyGate(CompositorProbeClient &client, const ProbeWindowTitles &titles,
                          CompositorWorkflowResult *result, QString *error)
{
    const auto before = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [](const WindowInventory &inventory) {
            return std::all_of(inventory.cbegin(), inventory.cend(), [](const auto &window) {
                return window.containerId.isEmpty() && !window.minimized;
            });
        },
        error);
    if (!before) {
        result->failure =
            QStringLiteral("read-only gate could not find independent probes: %1").arg(*error);
        return false;
    }
    const auto primary = before->value(titles.primary);
    const auto secondary = before->value(titles.secondary);
    const auto page = before->value(titles.page);

    const auto dockReply = client.dock(primary.id, secondary.id, 0.5, error);
    const auto releaseReply =
        client.call(QStringLiteral("ReleaseContainer"), QStringLiteral("not-a-container"), error);
    const auto submitReply = client.call(QStringLiteral("Submit"), QByteArrayLiteral("{}"), error);
    // AGENT-GUARD: Use malformed bytes to prove the production gate runs
    // before parsing; a parser error here would expose a test-only input seam.
    const auto injectReply = client.call(QStringLiteral("InjectTestInput"),
                                         QByteArrayLiteral("not-json"), error);
    const auto reinitializeReply = client.call(
        QStringLiteral("ReinitializeCompositingForTest"), error);
    const auto after = client.awaitWindows(
        {titles.primary, titles.secondary, titles.page},
        [&](const WindowInventory &inventory) {
            const auto currentPrimary = inventory.value(titles.primary);
            const auto currentSecondary = inventory.value(titles.secondary);
            const auto currentPage = inventory.value(titles.page);
            return currentPrimary.containerId.isEmpty() && currentSecondary.containerId.isEmpty() &&
                   currentPage.containerId.isEmpty() &&
                   sameGeometry(currentPrimary.frame, primary.frame) &&
                   sameGeometry(currentSecondary.frame, secondary.frame) &&
                   sameGeometry(currentPage.frame, page.frame);
        },
        error);
    const auto containersAfter = client.containers(error);
    if (!rejectedWith(dockReply, QLatin1StringView("control-disabled")) ||
        !rejectedWith(releaseReply, QLatin1StringView("control-disabled")) ||
        !rejectedWith(submitReply, QLatin1StringView("control-disabled")) ||
        !rejectedWith(injectReply, QLatin1StringView("control-disabled")) ||
        !rejectedWith(reinitializeReply, QLatin1StringView("control-disabled")) || !after ||
        !containersAfter || !containersAfter->isEmpty()) {
        result->failure = error->isEmpty()
                              ? QStringLiteral("production mutation gate did not reject atomically")
                              : *error;
        return false;
    }
    result->workflowPassed = true;
    result->evidence.insert(QStringLiteral("controlMode"), result->controlMode);
    result->evidence.insert(QStringLiteral("mutationsEnabled"), result->mutationsEnabled);
    result->evidence.insert(QStringLiteral("allMutatorsRejected"), true);
    result->evidence.insert(QStringLiteral("testInputRejectedBeforeParsing"), true);
    result->evidence.insert(QStringLiteral("compositorRestartRejected"), true);
    result->evidence.insert(QStringLiteral("threeWindowsUnchanged"), true);
    result->evidence.insert(QStringLiteral("ownershipRemainedClear"), true);
    return true;
}

} // namespace

CompositorWorkflowResult exerciseCompositorWorkflow(const QString &primaryTitle,
                                                    const QString &secondaryTitle,
                                                    const QString &pageTitle,
                                                    CompositorWorkflowMode mode,
                                                    const QString &dotoolPath,
                                                    std::function<void(const QString &)>
                                                        activateProbe,
                                                    std::function<void(const QString &)>
                                                        showPopupForProbe,
                                                    std::function<QString(const QString &)>
                                                        showDialogForProbe)
{
    CompositorWorkflowResult result;
    CompositorProbeClient client;
    QString error;
    if (!inspectEndpoint(client, mode, &result, &error)) {
        return result;
    }
    if (mode == CompositorWorkflowMode::InventoryOnly) {
        result.workflowPassed = true;
        return result;
    }

    const ProbeWindowTitles titles{primaryTitle, secondaryTitle, pageTitle};
    const auto decoratedWindows = awaitQindaDecorations(client, titles, &error);
    if (!decoratedWindows) {
        result.failure = QStringLiteral("QindaQt decoration runtime proof failed: %1").arg(error);
        return result;
    }
    QJsonArray decorationClasses;
    for (const auto &title : {titles.primary, titles.secondary, titles.page}) {
        decorationClasses.append(decoratedWindows->value(title).decorationClass);
    }
    result.evidence.insert(QStringLiteral("serverDecoratedWindowCount"),
                           static_cast<int>(decoratedWindows->size()));
    result.evidence.insert(QStringLiteral("decorationClasses"), decorationClasses);

    if (mode == CompositorWorkflowMode::ProductionReadOnly) {
        if (result.mutationsEnabled || result.controlMode != QStringLiteral("read-only")) {
            result.failure = QStringLiteral("production endpoint did not advertise read-only mode");
            return result;
        }
        exerciseReadOnlyGate(client, titles, &result, &error);
        return result;
    }
    if (!result.mutationsEnabled || result.controlMode != QStringLiteral("development-test")) {
        result.failure = QStringLiteral("development workflow mutations were not enabled");
        return result;
    }

    if (mode == CompositorWorkflowMode::HybridPointer) {
        auto pointerResult = exerciseHybridPointerWorkflow(
            client, titles, dotoolPath, activateProbe,
            showPopupForProbe, showDialogForProbe, &error);
        if (!pointerResult) {
            refreshHybridDiagnostics(client, &result);
            result.failure = error;
            return result;
        }
        for (auto iterator = pointerResult->evidence.constBegin();
             iterator != pointerResult->evidence.constEnd(); ++iterator) {
            result.evidence.insert(iterator.key(), iterator.value());
        }
        result.hybridDiagnostics = pointerResult->finalHybridDiagnostics;
        result.workflowPassed = true;
        return result;
    }

    auto evidence = exerciseDevelopmentWorkflow(client, titles, &error);
    if (!evidence) {
        result.failure = error;
        return result;
    }
    evidence->insert(QStringLiteral("inputObserverActive"), result.inputObserverActive);
    for (auto iterator = evidence->constBegin(); iterator != evidence->constEnd(); ++iterator) {
        result.evidence.insert(iterator.key(), iterator.value());
    }
    result.workflowPassed = true;
    return result;
}

} // namespace QindaQt::Test
