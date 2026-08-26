// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointergrouping.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Test {
namespace {

constexpr int InventoryTimeoutMilliseconds = 4000;

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

} // namespace

HybridPointerGrouping::HybridPointerGrouping(CompositorProbeClient &client,
                                             ProbeWindowTitles titles)
    : m_client(client)
    , m_titles(std::move(titles))
    , m_developmentInput(client)
{
}

bool HybridPointerGrouping::selectInputDriver(const QString &dotoolPath,
                                              const QPointF &initialPoint,
                                              const QRectF &output,
                                              QString *error)
{
    if (!m_dotool.start(dotoolPath, error)
        || !m_dotool.moveTo(initialPoint, output, error)) {
        return false;
    }
    processProbeEventsFor(500);

    QString admissionError;
    const auto dotoolDevices = awaitDotoolDevices(m_client, &admissionError);
    if (!m_dotool.isRunning()) {
        *error = QStringLiteral("dotool exited during KWin input admission; %1")
                     .arg(m_dotool.diagnostics());
        return false;
    }
    if (dotoolDevices) {
        m_injector = QStringLiteral("dotool-uinput");
        m_uinputAdmitted = true;
        m_uinputDevices = *dotoolDevices;
        m_drag = [this, output](const QPointF &start, const QPointF &end,
                               bool metaShift, QString *gestureError) {
            return m_dotool.drag(start, end, output, metaShift, gestureError);
        };
        m_activateFirstContextMenuAction =
            [this, output](const QPointF &point, QString *gestureError) {
                return m_dotool.activateFirstContextMenuAction(
                    point, output, gestureError);
            };
        return true;
    }

    // AGENT-NOTE: KWin --virtual may intentionally run without libinput, so a
    // writable /dev/uinput and healthy dotool process can remain invisible to
    // the nested seat. This fallback emits from a development-gated KWin
    // InputDevice through the same spy/filter/controller chain.
    m_injector = QStringLiteral("qindaqt-development-input");
    m_uinputAdmissionFailure = QStringLiteral("%1; %2")
                                   .arg(admissionError, m_dotool.diagnostics());
    m_drag = [this](const QPointF &start, const QPointF &end,
                    bool metaShift, QString *gestureError) {
        return m_developmentInput.drag(start, end, metaShift, gestureError);
    };
    m_activateFirstContextMenuAction =
        [this](const QPointF &point, QString *gestureError) {
            return m_developmentInput.activateFirstContextMenuAction(
                point, gestureError);
        };
    return true;
}

std::optional<HybridPointerGroupedState>
HybridPointerGrouping::group(const QString &dotoolPath, QString *error)
{
    error->clear();
    if (m_groupAttempted) {
        *error = QStringLiteral("Hybrid pointer grouping may run only once per input session");
        return std::nullopt;
    }
    m_groupAttempted = true;
    const QStringList probeTitles{m_titles.primary, m_titles.secondary, m_titles.page};
    const auto initial = m_client.awaitWindows(
        probeTitles,
        [](const WindowInventory &inventory) {
            return std::all_of(inventory.cbegin(), inventory.cend(),
                               [](const ObservedWindow &observed) {
                                   return observed.containerId.isEmpty()
                                       && !observed.minimized
                                       && observed.frame.isValid()
                                       && observed.targetFrame.isValid();
                               });
        }, error, InventoryTimeoutMilliseconds);
    if (!initial) {
        *error = QStringLiteral("could not discover independent painted probes: %1")
                     .arg(*error);
        return std::nullopt;
    }
    const auto initialHybrid = readHybridDiagnostics(m_client, error);
    const auto output = singleOutputFrame(m_client, error);
    if (!initialHybrid || !output) {
        return std::nullopt;
    }
    if (initialHybrid->containerCount != 0) {
        *error = QStringLiteral("Hybrid pointer proof did not start with zero containers");
        return std::nullopt;
    }
    const auto gesture = chooseDockGesture(*initial, m_titles, *output, error);
    if (!gesture) {
        return std::nullopt;
    }
    const auto bystander = bystanderTitle(m_titles, *gesture);
    if (bystander.isEmpty()) {
        *error = QStringLiteral("Hybrid pointer workflow could not identify its bystander probe");
        return std::nullopt;
    }

    if (!selectInputDriver(dotoolPath, gesture->sourcePoint, *output, error)
        || !drag(gesture->sourcePoint, gesture->dropPoint, true, error)) {
        return std::nullopt;
    }
    const auto grouped = m_client.awaitWindows(
        probeTitles,
        [&](const WindowInventory &inventory) {
            const auto &source = window(inventory, gesture->sourceTitle);
            const auto &target = window(inventory, gesture->targetTitle);
            return !source.containerId.isEmpty()
                && source.containerId == target.containerId
                && splitEvidence(source.targetFrame, target.targetFrame).valid
                && window(inventory, bystander).containerId.isEmpty();
        }, error, InventoryTimeoutMilliseconds);
    if (!grouped) {
        *error = QStringLiteral("Meta+Shift+left pointer dock did not commit: %1; %2")
                     .arg(*error, m_dotool.diagnostics());
        return std::nullopt;
    }
    const auto groupedHybrid = awaitHybridDiagnostics(
        m_client,
        [&](const HybridDiagnostics &diagnostics) {
            return diagnostics.revision > initialHybrid->revision
                && diagnostics.containerCount == 1
                && diagnostics.json.value(QStringLiteral("chromeOverlayCount")).toInt(-1) == 1
                && diagnostics.json
                       .value(QStringLiteral("visibleAnchoredChromeSceneItemCount"))
                       .toInt(-1) == 1;
        }, error);
    if (!groupedHybrid) {
        return std::nullopt;
    }
    const auto &source = window(*grouped, gesture->sourceTitle);
    const auto &target = window(*grouped, gesture->targetTitle);
    const auto publicContainer = readPublicHybridContainer(
        m_client, source.containerId, groupedHybrid->revision, error);
    const auto split = splitEvidence(source.targetFrame, target.targetFrame);
    if (!publicContainer || !split.valid) {
        return std::nullopt;
    }
    return HybridPointerGroupedState{*initial, *grouped, *initialHybrid,
                                     *groupedHybrid, *publicContainer,
                                     *gesture, split, bystander, *output};
}

bool HybridPointerGrouping::drag(const QPointF &start,
                                 const QPointF &end,
                                 bool metaShift,
                                 QString *error)
{
    if (!m_dotool.isRunning()) {
        *error = QStringLiteral("dotool did not remain alive for the gesture; %1")
                     .arg(m_dotool.diagnostics());
        return false;
    }
    if (!m_drag) {
        *error = QStringLiteral("Hybrid pointer input driver was not selected");
        return false;
    }
    return m_drag(start, end, metaShift, error);
}

bool HybridPointerGrouping::activateFirstContextMenuAction(
    const QPointF &point,
    QString *error)
{
    if (!m_dotool.isRunning()) {
        *error = QStringLiteral("dotool did not remain alive for the context menu; %1")
                     .arg(m_dotool.diagnostics());
        return false;
    }
    if (!m_activateFirstContextMenuAction) {
        *error = QStringLiteral("Hybrid context-menu input driver was not selected");
        return false;
    }
    return m_activateFirstContextMenuAction(point, error);
}

bool HybridPointerGrouping::dotoolRunning() const
{
    return m_dotool.isRunning();
}

QString HybridPointerGrouping::dotoolDiagnostics() const
{
    return m_dotool.diagnostics();
}

QJsonObject HybridPointerGrouping::inputEvidence() const
{
    return {{QStringLiteral("inputInjector"), m_injector},
            {QStringLiteral("dotoolProcessCount"), 1},
            {QStringLiteral("dotoolProcessStayedRunning"), m_dotool.isRunning()},
            {QStringLiteral("uinputAdmitted"), m_uinputAdmitted},
            {QStringLiteral("uinputDevices"), m_uinputDevices},
            {QStringLiteral("uinputAdmissionFailure"), m_uinputAdmissionFailure},
            {QStringLiteral("developmentInputDeviceId"),
             m_developmentInput.deviceId()},
            {QStringLiteral("developmentInputRequestCount"),
             m_developmentInput.requestCount()}};
}

} // namespace QindaQt::Test
