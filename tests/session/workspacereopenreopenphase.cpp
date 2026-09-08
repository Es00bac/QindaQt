// SPDX-License-Identifier: GPL-3.0-or-later
#include "workspacereopenphases.h"

#include "compositorprobeclient.h"
#include "hybridpointerinventory.h"
#include "hybridtestinputdriver.h"
#include "workspacereopeninput.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <memory>

namespace QindaQt::Test {
namespace {

const QString TerminalAlphaTitle = QStringLiteral("WReopen Term Alpha");
const QString TerminalBetaTitle = QStringLiteral("WReopen Term Beta");
const QString EditorTitle = QStringLiteral("WReopen Editor");
const QString TerminalAppId = QStringLiteral("org.qindaqt.wreopen-terminal");
const QString PhantomAppId = QStringLiteral("org.qindaqt.wreopen-phantom");
const QString EditorAppId = QStringLiteral("org.qindaqt.wreopen-editor");

struct ReopenFixture final
{
    QString workspaceId;
    QString name;
    QString color;
    // Slot ids in document order and the window title each must receive.
    // Terminal slots take the two same-app windows (document order), the
    // phantom slot takes the replacement editor window.
    QStringList slotOrder;
    QHash<QString, QString> slotTitles;
};

std::optional<ReopenFixture> parseFixture(const QJsonObject &document,
                                          QString *error)
{
    ReopenFixture fixture{
        document.value(QStringLiteral("id")).toString(),
        document.value(QStringLiteral("name")).toString(),
        document.value(QStringLiteral("color")).toString(), {}, {}};
    const auto applications =
        document.value(QStringLiteral("applications")).toArray();
    QStringList terminalSlots;
    for (const auto &entry : applications) {
        const auto object = entry.toObject();
        const auto slotId = object.value(QStringLiteral("id")).toString();
        const auto appId =
            object.value(QStringLiteral("desktopEntryId")).toString();
        fixture.slotOrder.append(slotId);
        if (appId == TerminalAppId) {
            terminalSlots.append(slotId);
        } else if (appId == PhantomAppId) {
            fixture.slotTitles.insert(slotId, EditorTitle);
        } else {
            *error = QStringLiteral("unexpected desktop entry '%1' in saved workspace")
                         .arg(appId);
            return std::nullopt;
        }
    }
    if (fixture.workspaceId.isEmpty() || fixture.name.isEmpty()
        || fixture.color.isEmpty() || fixture.slotOrder.size() != 3
        || terminalSlots.size() != 2
        || fixture.slotTitles.size() != 1) {
        *error = QStringLiteral(
            "saved workspace is not the session-1 fixture (two same-app slots "
            "plus one phantom slot)");
        return std::nullopt;
    }
    fixture.slotTitles.insert(terminalSlots.at(0), TerminalAlphaTitle);
    fixture.slotTitles.insert(terminalSlots.at(1), TerminalBetaTitle);
    return fixture;
}

// Combo item zero is "Choose a window"; eligible windows follow in the
// compositor's sorted-id inventory order, which availableWindows() reproduces.
int choiceIndex(const QJsonArray &eligible, const QString &title,
                QString *error)
{
    for (qsizetype index = 0; index < eligible.size(); ++index) {
        if (eligible.at(index).toObject().value(QStringLiteral("title")).toString()
            == title) {
            return int(index) + 1;
        }
    }
    *error = QStringLiteral("window '%1' missing from the eligible inventory")
                 .arg(title);
    return -1;
}

bool layoutMatchesWithMapping(const QJsonObject &savedNode,
                              const QJsonObject &liveNode,
                              const QHash<QString, QString> &slotToWindow)
{
    const auto type = savedNode.value(QStringLiteral("type")).toString();
    if (type != liveNode.value(QStringLiteral("type")).toString()) {
        return false;
    }
    if (type == QStringLiteral("leaf")) {
        const auto slotId =
            savedNode.value(QStringLiteral("windowId")).toString();
        return slotToWindow.value(slotId)
            == liveNode.value(QStringLiteral("windowId")).toString();
    }
    if (savedNode.value(QStringLiteral("orientation"))
        != liveNode.value(QStringLiteral("orientation"))) {
        return false;
    }
    const auto savedRatio = savedNode.value(QStringLiteral("ratio")).toDouble(-1);
    const auto liveRatio = liveNode.value(QStringLiteral("ratio")).toDouble(-2);
    if (savedRatio < 0.0 || qAbs(savedRatio - liveRatio) > 1e-9) {
        return false;
    }
    return layoutMatchesWithMapping(
               savedNode.value(QStringLiteral("first")).toObject(),
               liveNode.value(QStringLiteral("first")).toObject(), slotToWindow)
        && layoutMatchesWithMapping(
            savedNode.value(QStringLiteral("second")).toObject(),
            liveNode.value(QStringLiteral("second")).toObject(), slotToWindow);
}

bool restoredLayoutMatches(const QJsonObject &document,
                           const QJsonObject &liveContainer,
                           const QHash<QString, QString> &slotToWindow,
                           QString *error)
{
    const auto savedPages = document.value(QStringLiteral("layout")).toObject()
                                .value(QStringLiteral("pages")).toArray();
    const auto livePages = liveContainer.value(QStringLiteral("pages")).toArray();
    if (savedPages.size() != livePages.size() || savedPages.isEmpty()) {
        *error = QStringLiteral("restored container page count differs from the saved layout");
        return false;
    }
    for (qsizetype index = 0; index < savedPages.size(); ++index) {
        if (!layoutMatchesWithMapping(
                savedPages.at(index).toObject().value(QStringLiteral("root")).toObject(),
                livePages.at(index).toObject().value(QStringLiteral("root")).toObject(),
                slotToWindow)) {
            *error = QStringLiteral("restored page %1 does not match the saved layout")
                         .arg(index);
            return false;
        }
    }
    return true;
}

bool expectNoContainerAdoption(CompositorProbeClient &client, int milliseconds,
                               QString *error)
{
    // Behavioural negative control: with the duplicate same-app slots and the
    // missing-application slot not all explicitly assigned, the production
    // dialog must never auto-complete and restore.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        const auto containers = client.containers(error);
        if (!containers) {
            return false;
        }
        for (const auto &entry : *containers) {
            if (entry.toObject().value(QStringLiteral("authority")).toString()
                == QStringLiteral("hybrid-process")) {
                *error = QStringLiteral(
                    "a container was adopted before the assignment plan was complete");
                return false;
            }
        }
        processProbeEventsFor(60);
    }
    return true;
}

bool assignSlotChoices(WorkspaceReopenInput &input, const QJsonArray &eligible,
                       const ReopenFixture &fixture, int firstSlot,
                       int slotCount, QString *error)
{
    for (int slot = firstSlot; slot < firstSlot + slotCount; ++slot) {
        const auto title = fixture.slotTitles.value(fixture.slotOrder.at(slot));
        const int index = choiceIndex(eligible, title, error);
        if (index < 0) {
            return false;
        }
        if (slot > 0) {
            const auto previousTitle = fixture.slotTitles.value(fixture.slotOrder.at(slot - 1));
            const int tabs = previousTitle == EditorTitle ? 1 : 2;
            for (int step = 0; step < tabs; ++step) {
                if (!input.pressKey(QStringLiteral("tab"), error)) return false;
            }
        }
        // Choose in the popup so intermediate navigation does not commit a
        // transient assignment or trigger automatic matching between arrows.
        if (!input.pressKey(QStringLiteral("space"), error)) return false;
        // assignWindows may prefill a uniquely matched row. Clamp to the
        // explicit "Choose a window" entry before counting the target index,
        // otherwise Down navigation becomes relative to that automatic choice.
        for (qsizetype step = 0; step <= eligible.size(); ++step) {
            if (!pressSequence(input, {QStringLiteral("up")}, error)) {
                return false;
            }
        }
        for (int step = 0; step < index; ++step) {
            if (!pressSequence(input, {QStringLiteral("down")}, error)) {
                return false;
            }
        }
        if (!input.pressKey(QStringLiteral("enter"), error)) return false;
    }
    return true;
}

std::optional<QJsonArray> reopenEligibleChoices(CompositorProbeClient &client,
                                                QString *error)
{
    const auto eligible = awaitEligibleWindows(
        client, {TerminalAlphaTitle, TerminalBetaTitle, EditorTitle}, error);
    if (!eligible) {
        return std::nullopt;
    }
    if (eligible->size() != 3) {
        *error = QStringLiteral(
            "eligible inventory holds %1 windows; exactly the three fixture "
            "clients were expected")
                     .arg(eligible->size());
        return std::nullopt;
    }
    return eligible;
}

std::optional<QJsonObject> verifyAdoption(
    CompositorProbeClient &client, const QJsonObject &document,
    const ReopenFixture &fixture, const QHash<QString, QString> &windowIds,
    QString *error)
{
    const QStringList titles{TerminalAlphaTitle, TerminalBetaTitle, EditorTitle};
    const auto adopted = client.awaitWindows(
        titles,
        [](const WindowInventory &inventory) {
            const auto first = inventory.cbegin();
            return !first->containerId.isEmpty()
                && std::all_of(inventory.cbegin(), inventory.cend(),
                               [first](const ObservedWindow &observed) {
                                   return observed.containerId
                                       == first->containerId;
                               });
        },
        error, 6000);
    if (!adopted) {
        return std::nullopt;
    }
    const QString containerId = adopted->cbegin()->containerId;
    const auto containers = client.containers(error);
    if (!containers || containers->size() != 1
        || containers->at(0).toObject().value(QStringLiteral("id")).toString()
            != containerId
        || containers->at(0).toObject().value(QStringLiteral("authority")).toString()
            != QStringLiteral("hybrid-process")) {
        *error = QStringLiteral(
            "Containers did not publish exactly the adopted Hybrid container");
        return std::nullopt;
    }
    const auto snapshotReply = client.call(QStringLiteral("Snapshot"), containerId, error);
    if (!snapshotReply
        || snapshotReply->value(QStringLiteral("status")).toString()
            != QStringLiteral("ok")) {
        return std::nullopt;
    }
    QHash<QString, QString> slotToWindow;
    for (auto it = fixture.slotTitles.cbegin(); it != fixture.slotTitles.cend();
         ++it) {
        slotToWindow.insert(it.key(), windowIds.value(it.value()));
    }
    if (!restoredLayoutMatches(
            document,
            snapshotReply->value(QStringLiteral("snapshot")).toObject(),
            slotToWindow, error)) {
        return std::nullopt;
    }
    return QJsonObject{{QStringLiteral("containerId"), containerId},
                       {QStringLiteral("layoutMatchesSavedDocument"), true}};
}

bool resaveRestoredContainer(WorkspaceReopenInput &input,
                             CompositorProbeClient &client,
                             const ReopenFixture &fixture, QString *error)
{
    // The library dialog stayed open behind the accepted Reopen dialog with
    // focus on its Reopen button; Shift+Tab lands on "Save current". Saving
    // prefills from the restored container's presentation, so the document
    // only keeps its name/color if the restore genuinely applied them.
    if (!input.pressChord({QStringLiteral("left-shift"), QStringLiteral("tab")},
                          error)
        || !pressSequence(input, {QStringLiteral("space")}, error)) {
        return false;
    }
    // Internal Qt dialogs are not published through the client inventory.
    // The native Save button center is stable in this fixed 1920x1080 row.
    processProbeEventsFor(400);
    if (!input.movePointer({994.0, 568.0}, error)
        || !input.clickButton(QLatin1StringView("left"), error)) {
        return false;
    }
    if (!awaitWindowTitleGone(client, QStringLiteral("Save workspace"), error)) {
        return false;
    }
    const auto updated = readSavedWorkspaceDocument(error);
    if (!updated) {
        return false;
    }
    if (updated->value(QStringLiteral("id")).toString() != fixture.workspaceId
        || updated->value(QStringLiteral("name")).toString() != fixture.name
        || updated->value(QStringLiteral("color")).toString() != fixture.color) {
        *error = QStringLiteral(
            "re-saving the restored container did not preserve the workspace "
            "identity, name, and color");
        return false;
    }
    QStringList entryIds;
    for (const auto &entry :
         updated->value(QStringLiteral("applications")).toArray()) {
        entryIds.append(
            entry.toObject().value(QStringLiteral("desktopEntryId")).toString());
    }
    entryIds.sort();
    const QStringList expected{EditorAppId, TerminalAppId, TerminalAppId};
    if (entryIds != expected) {
        *error = QStringLiteral(
            "restored container applications %1 do not record the explicit "
            "editor replacement for the missing application")
                     .arg(entryIds.join(QLatin1Char(',')));
        return false;
    }
    return true;
}

} // namespace

std::optional<QJsonObject> exerciseWorkspaceReopenPhase(
    CompositorProbeClient &client, QString *error)
{
    const auto document = readSavedWorkspaceDocument(error);
    if (!document) {
        return std::nullopt;
    }
    const auto fixture = parseFixture(*document, error);
    if (!fixture) {
        return std::nullopt;
    }
    const QVector<WorkspaceClientSpec> specs{
        {EditorAppId, {EditorTitle}, {QSize(520, 360)}},
        {TerminalAppId,
         {TerminalAlphaTitle, TerminalBetaTitle},
         {QSize(560, 400), QSize(480, 320)}},
    };
    auto clients = std::make_unique<QObject>();
    if (spawnWorkspaceClients(specs, {}, clients.get(), error).isEmpty()) {
        return std::nullopt;
    }
    const QStringList titles{TerminalAlphaTitle, TerminalBetaTitle, EditorTitle};
    const auto initial = client.awaitWindows(
        titles,
        [](const WindowInventory &inventory) {
            return std::all_of(inventory.cbegin(), inventory.cend(),
                               [](const ObservedWindow &observed) {
                                   return observed.containerId.isEmpty()
                                       && !observed.minimized
                                       && observed.frame.isValid();
                               });
        },
        error, 6000);
    if (!initial) {
        return std::nullopt;
    }
    QHash<QString, QString> windowIds;
    for (const QString &title : titles) {
        windowIds.insert(title, initial->value(title).id);
    }

    WorkspaceReopenInput input(client);
    // Meta+Ctrl+W opens the parentless modal library. Its Qt Widgets surface
    // is intentionally not part of the client-window inventory.
    if (!input.pressChord({QStringLiteral("left-meta"),
                           QStringLiteral("left-control"), QStringLiteral("w")},
                          error)) {
        return std::nullopt;
    }
    processProbeEventsFor(400);
    // The library's native Reopen button center is stable in the fixed
    // 1920x1080 scenario. The child dialog itself then owns keyboard focus.
    if (!input.movePointer({1032.0, 687.0}, error)
        || !input.clickButton(QLatin1StringView("left"), error)) {
        return std::nullopt;
    }
    processProbeEventsFor(400);
    const auto eligible = reopenEligibleChoices(client, error);
    if (!eligible) {
        return std::nullopt;
    }
    // Assign the first two rows in their actual layout order. No container
    // may be adopted while the user is still editing the choices.
    if (!assignSlotChoices(input, *eligible, *fixture, 0, 2, error)) {
        return std::nullopt;
    }
    if (!expectNoContainerAdoption(client, 800, error)) {
        return std::nullopt;
    }
    if (!assignSlotChoices(input, *eligible, *fixture, 2, 1, error)) {
        return std::nullopt;
    }
    const int restoreTabs = fixture->slotTitles.value(fixture->slotOrder.last()) == EditorTitle ? 2 : 3;
    for (int step = 0; step < restoreTabs; ++step) {
        if (!input.pressKey(QStringLiteral("tab"), error)) return std::nullopt;
    }
    if (!input.pressKey(QStringLiteral("space"), error)) return std::nullopt;
    const auto adoption = verifyAdoption(client, *document, *fixture, windowIds,
                                         error);
    if (!adoption) {
        return std::nullopt;
    }
    if (!resaveRestoredContainer(input, client, *fixture, error)) {
        return std::nullopt;
    }
    auto evidence = *adoption;
    evidence.insert(QStringLiteral("phase"), QStringLiteral("reopen"));
    evidence.insert(QStringLiteral("workspaceId"), fixture->workspaceId);
    evidence.insert(QStringLiteral("workspaceName"), fixture->name);
    evidence.insert(QStringLiteral("workspaceColor"), fixture->color);
    evidence.insert(QStringLiteral("missingApplicationReported"), PhantomAppId);
    evidence.insert(QStringLiteral("replacementApplication"), EditorAppId);
    evidence.insert(QStringLiteral("duplicateSlotsExplicitlyAssigned"), true);
    evidence.insert(QStringLiteral("nameColorRoundTripThroughRestore"), true);
    evidence.insert(QStringLiteral("developmentInputDeviceId"), input.deviceId());
    evidence.insert(QStringLiteral("developmentInputRequests"),
                    input.requestCount());
    return evidence;
}

} // namespace QindaQt::Test
