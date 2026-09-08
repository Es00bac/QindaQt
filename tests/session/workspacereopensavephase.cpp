// SPDX-License-Identifier: GPL-3.0-or-later
#include "workspacereopenphases.h"

#include "compositorprobeclient.h"
#include "hybridpointergeometry.h"
#include "hybridpointergrouping.h"
#include "hybridpointerinventory.h"
#include "workspacereopeninput.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <array>
#include <memory>

namespace QindaQt::Test {
namespace {

// AGENT-NOTE: Fixture geometry assumes the virtual backend's documented
// at-origin placement. The phantom window starts largest so its decoration
// title stays grabbable after the two terminal windows stack above it, then
// moves aside so the dock gestures have clear edges. Sizes must stay
// distinct and ordered: Term One (topmost) < Term Two < Phantom.
const QString TerminalOneTitle = QStringLiteral("WReopen Term One");
const QString TerminalTwoTitle = QStringLiteral("WReopen Term Two");
const QString PhantomTitle = QStringLiteral("WReopen Phantom");
const QString TerminalAppId = QStringLiteral("org.qindaqt.wreopen-terminal");
const QString PhantomAppId = QStringLiteral("org.qindaqt.wreopen-phantom");
const QString FixtureName = QStringLiteral("cvn");
const QString FixtureColor = QStringLiteral("#30A46C"); // "Green" menu swatch

// Top-level group context menu positions at the production base: Down presses
// from a freshly popped menu with no preselected action. Separators and
// disabled entries are skipped by Qt's own navigation.
constexpr int MenuDownsSavedWorkspaces = 1;
constexpr int MenuDownsRename = 7;
constexpr int MenuDownsGroupColor = 8;
// A keyboard-opened submenu preselects its first action ("Default"); "Green"
// is the fifth entry (Default, Red, Orange, Yellow, Green).
constexpr int SubmenuDownsGreen = 4;

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

std::optional<QPointF> titleGrabPoint(const WindowInventory &inventory,
                                      const QString &title,
                                      const QRectF &output)
{
    // The press lands on whatever window is topmost at the point, so a grab
    // candidate is only safe when no other fixture window's frame covers it.
    const auto &observed = window(inventory, title);
    for (const qreal fraction : {0.50, 0.72, 0.28, 0.86, 0.94, 0.14, 0.06}) {
        const QPointF candidate(
            observed.frame.left() + observed.frame.width() * fraction,
            observed.frame.top() + 15.0);
        bool clear = observed.frame.contains(candidate)
            && output.contains(candidate);
        for (const auto &other : inventory) {
            if (other.title != title && other.frame.contains(candidate)) {
                clear = false;
            }
        }
        if (clear) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<QPointF> memberEdgeDropPoint(
    const WindowInventory &inventory, const QStringList &memberTitles,
    const QString &draggedTitle, const QRectF &output)
{
    for (const QString &memberTitle : memberTitles) {
        const auto &member = window(inventory, memberTitle);
        for (const qreal fraction : {0.50, 0.68, 0.32, 0.82, 0.18}) {
            const std::array<QPointF, 4> candidates{{
                {member.frame.left() + member.frame.width() * 0.08,
                 member.frame.top() + member.frame.height() * fraction},
                {member.frame.left() + member.frame.width() * fraction,
                 member.frame.bottom() - member.frame.height() * 0.08},
                {member.frame.left() + member.frame.width() * fraction,
                 member.frame.top() + member.frame.height() * 0.08},
                {member.frame.right() - member.frame.width() * 0.08,
                 member.frame.top() + member.frame.height() * fraction},
            }};
            for (const auto &candidate : candidates) {
                bool clear = member.frame.contains(candidate)
                    && output.contains(candidate);
                for (const auto &other : inventory) {
                    if (other.title != memberTitle && other.title != draggedTitle
                        && other.frame.contains(candidate)) {
                        clear = false;
                    }
                }
                if (clear) {
                    return candidate;
                }
            }
        }
    }
    return std::nullopt;
}

QPointF groupTitlePoint(const WindowInventory &inventory,
                        const QStringList &memberTitles)
{
    QRectF bounds;
    for (const QString &title : memberTitles) {
        const auto frame = window(inventory, title).targetFrame;
        bounds = bounds.isValid() ? bounds.united(frame) : frame;
    }
    // Mirrors sharedTitleCenter: one outer border, then the 29px shared title
    // row above the member frames.
    const auto outer = bounds.adjusted(-1.0, -29.0, 1.0, 1.0);
    const QRectF titleRect(outer.left() + 8.0, outer.top() + 4.0,
                           outer.width() - 16.0, 26.0);
    // The center of a multi-member row is occupied by a tab. Use the
    // reserved outer-title drag strip beside the button cluster so the
    // pointer router publishes a container context menu rather than a tab
    // request carrying a member id.
    return {titleRect.left() + titleRect.width() * 0.15,
            titleRect.center().y()};
}

bool movePhantomAside(WorkspaceReopenInput &input,
                      const WindowInventory &inventory, const QRectF &output,
                      QString *error)
{
    const auto &phantom = window(inventory, PhantomTitle);
    const QPointF grab(phantom.frame.center().x(), phantom.frame.top() + 12.0);
    if (!phantom.frame.contains(grab) || !output.contains(grab)) {
        *error = QStringLiteral("phantom fixture window has no clear title point");
        return false;
    }
    // Use the decoration title's upper strip, matching the native move rows;
    // dropping near the upper-right corner moves the large phantom clear of
    // both terminal surfaces instead of leaving its frame under their edges.
    const QPointF destination(output.right() - 100.0, output.top() + 100.0);
    return input.drag(grab, destination, false, error);
}

bool awaitPhantomSeparated(CompositorProbeClient &client,
                           const QRectF &output, QString *error)
{
    const QStringList titles{TerminalOneTitle, TerminalTwoTitle, PhantomTitle};
    // AGENT-GUARD: the generic chooser intentionally tries page-to-primary
    // first, but only after the bystander has finished its move. Without this
    // observation the still-overlapping phantom can win the target hit test,
    // producing a valid but wrong group (Term One + Phantom) and leaving Term
    // Two standalone before the saved-workspace assertions run.
    const auto separated = client.awaitWindows(
        titles,
        [output](const WindowInventory &current) {
            const auto &phantom = window(current, PhantomTitle);
            const auto &termOne = window(current, TerminalOneTitle);
            const auto &termTwo = window(current, TerminalTwoTitle);
            return phantom.containerId.isEmpty()
                && phantom.frame.isValid()
                && output.contains(phantom.frame.center())
                && phantom.frame.center().x() > output.center().x()
                && !phantom.frame.intersects(termOne.frame)
                && !phantom.frame.intersects(termTwo.frame);
        },
        error, 4000);
    if (!separated) {
        *error = QStringLiteral(
            "phantom fixture did not settle clear of both terminal windows: %1")
                     .arg(*error);
        return false;
    }
    return true;
}

std::optional<WindowInventory> awaitThreeGrouped(
    CompositorProbeClient &client, const QString &containerId, QString *error)
{
    const QStringList titles{TerminalOneTitle, TerminalTwoTitle, PhantomTitle};
    return client.awaitWindows(
        titles,
        [containerId](const WindowInventory &inventory) {
            return std::all_of(
                inventory.cbegin(), inventory.cend(),
                [containerId](const ObservedWindow &observed) {
                    return observed.containerId == containerId
                        && observed.targetFrame.isValid();
                });
        },
        error, 4000);
}

bool applyFixturePresentation(WorkspaceReopenInput &input,
                              const QPointF &menuPoint, QString *error)
{
    // Group Color ▸ Green, then Rename…, both through the real chrome menu.
    if (!openMenuAt(input, menuPoint, error)
        || !chooseSubmenuItem(input, MenuDownsGroupColor, SubmenuDownsGreen,
                              error)) {
        *error = QStringLiteral("group color menu selection failed: %1").arg(*error);
        return false;
    }
    // The workspace Save dialog owns the name. Leave KWin's independent
    // group-rename dialog out of this proof so every typed character below
    // exercises the controller-owned internal-dialog keyboard bridge.
    processProbeEventsFor(400);
    return true;
}

bool saveThroughLibrary(WorkspaceReopenInput &input, const QPointF &menuPoint,
                        QString *error)
{
    if (!openMenuAt(input, menuPoint, error)
        || !chooseMenuItem(input, MenuDownsSavedWorkspaces, error)) {
        *error = QStringLiteral("saved workspaces menu selection failed: %1").arg(*error);
        return false;
    }
    // Compositor-owned dialogs are not ordinary application inventory entries.
    // The saved document below verifies that the UI action actually completed.
    processProbeEventsFor(400);
    // KWin's internal Widgets surface is not published in the client
    // inventory. These centers were observed from its real layout in the fixed
    // 1920x1080 scenario; use pointer activation for native buttons while the
    // typed workspace name below exercises the controller's keyboard bridge.
    if (!input.movePointer({935.0, 687.0}, error)
        || !input.clickButton(QLatin1StringView("left"), error)) {
        *error = QStringLiteral("Save current pointer activation failed: %1").arg(*error);
        return false;
    }
    processProbeEventsFor(400);
    // SaveDialog is a compositor-owned internal widget. Explicitly point at
    // its name edit before typing so this path proves the controller bridge,
    // rather than assuming focus transfers across the native child dialog.
    if (!input.movePointer({1000.0, 510.0}, error)
        || !input.clickButton(QLatin1StringView("left"), error)
        || !typeFixtureName(input, FixtureName, error)
        || !input.movePointer({994.0, 568.0}, error)
        || !input.clickButton(QLatin1StringView("left"), error)) {
        *error = QStringLiteral("Save workspace input failed: %1").arg(*error);
        return false;
    }
    return awaitWindowTitleGone(input.client(), QStringLiteral("Save workspace"),
                                error);
}

bool documentMatchesFixture(const QJsonObject &document, QString *error)
{
    const auto name = document.value(QStringLiteral("name")).toString();
    const auto color = document.value(QStringLiteral("color")).toString();
    const auto applications =
        document.value(QStringLiteral("applications")).toArray();
    if (!name.endsWith(FixtureName) || name.size() > 128) {
        *error = QStringLiteral(
            "saved workspace name '%1' does not carry the typed fixture name")
                     .arg(name);
        return false;
    }
    if (color != FixtureColor) {
        *error = QStringLiteral(
            "saved workspace color '%1' is not the menu-chosen %2")
                     .arg(color, FixtureColor);
        return false;
    }
    QStringList entryIds;
    for (const auto &entry : applications) {
        entryIds.append(
            entry.toObject().value(QStringLiteral("desktopEntryId")).toString());
    }
    entryIds.sort();
    const QStringList expected{PhantomAppId, TerminalAppId, TerminalAppId};
    if (entryIds != expected) {
        *error = QStringLiteral(
            "saved workspace applications %1 are not the two same-app slots "
            "plus the phantom slot")
                     .arg(entryIds.join(QLatin1Char(',')));
        return false;
    }
    return true;
}

} // namespace

std::optional<QJsonObject> exerciseWorkspaceSavePhase(
    CompositorProbeClient &client, QString *error)
{
    const QVector<WorkspaceClientSpec> specs{
        {PhantomAppId, {PhantomTitle}, {QSize(720, 520)}},
        {TerminalAppId,
         {TerminalTwoTitle, TerminalOneTitle},
         {QSize(640, 480), QSize(400, 280)}},
    };
    auto clients = std::make_unique<QObject>();
    if (spawnWorkspaceClients(specs, {TerminalOneTitle}, clients.get(), error)
            .isEmpty()) {
        return std::nullopt;
    }
    const QStringList titles{TerminalOneTitle, TerminalTwoTitle, PhantomTitle};
    const auto initial = client.awaitWindows(
        titles,
        [](const WindowInventory &inventory) {
            return std::all_of(inventory.cbegin(), inventory.cend(),
                               [](const ObservedWindow &observed) {
                                   return observed.containerId.isEmpty()
                                       && !observed.minimized
                                       && observed.frame.isValid()
                                       && observed.targetFrame.isValid();
                               });
        },
        error, 6000);
    const auto output = singleOutputFrame(client, error);
    if (!initial || !output) {
        return std::nullopt;
    }

    WorkspaceReopenInput input(client);
    if (!movePhantomAside(input, *initial, *output, error)) {
        return std::nullopt;
    }
    if (!awaitPhantomSeparated(client, *output, error)) {
        return std::nullopt;
    }
    HybridPointerGrouping grouping(client, ProbeWindowTitles{
                                               TerminalTwoTitle, PhantomTitle,
                                               TerminalOneTitle});
    grouping.forceDevelopmentInput(true);
    const auto grouped = grouping.group(QString{}, error);
    if (!grouped) {
        return std::nullopt;
    }
    const QString containerId =
        window(grouped->grouped, TerminalOneTitle).containerId;

    const auto beforeThirdDock = client.awaitWindows(
        titles,
        [containerId](const WindowInventory &inventory) {
            return window(inventory, PhantomTitle).containerId.isEmpty()
                && window(inventory, PhantomTitle).frame.isValid();
        },
        error, 4000);
    if (!beforeThirdDock) {
        return std::nullopt;
    }
    const auto grab = titleGrabPoint(*beforeThirdDock, PhantomTitle, *output);
    const auto drop = memberEdgeDropPoint(*beforeThirdDock,
                                          {TerminalOneTitle, TerminalTwoTitle},
                                          PhantomTitle, *output);
    if (!grab || !drop
        || !grouping.drag(*grab, *drop, true, error)) {
        if (error->isEmpty()) {
            *error = QStringLiteral(
                "no clear title-to-member-edge path for the third dock");
        }
        return std::nullopt;
    }
    const auto groupedAll = awaitThreeGrouped(client, containerId, error);
    if (!groupedAll) {
        return std::nullopt;
    }

    const auto menuPoint = groupTitlePoint(*groupedAll, titles);
    if (!applyFixturePresentation(input, menuPoint, error)
        || !saveThroughLibrary(input, menuPoint, error)) {
        return std::nullopt;
    }
    const auto document = readSavedWorkspaceDocument(error);
    if (!document || !documentMatchesFixture(*document, error)) {
        return std::nullopt;
    }
    return QJsonObject{
        {QStringLiteral("phase"), QStringLiteral("save")},
        {QStringLiteral("workspaceId"),
         document->value(QStringLiteral("id")).toString()},
        {QStringLiteral("workspaceName"),
         document->value(QStringLiteral("name")).toString()},
        {QStringLiteral("workspaceColor"),
         document->value(QStringLiteral("color")).toString()},
        {QStringLiteral("containerId"), containerId},
        {QStringLiteral("developmentInputDeviceId"), input.deviceId()},
        {QStringLiteral("developmentInputRequests"), input.requestCount()},
        {QStringLiteral("members"),
         QJsonArray{window(*groupedAll, TerminalOneTitle).id,
                    window(*groupedAll, TerminalTwoTitle).id,
                    window(*groupedAll, PhantomTitle).id}},
    };
}

} // namespace QindaQt::Test
