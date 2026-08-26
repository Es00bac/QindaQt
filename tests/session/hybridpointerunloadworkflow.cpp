// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridpointerunloadworkflow.h"

#include "compositorprobeclient.h"
#include "hybridpointergrouping.h"
#include "hybridpointerinventory.h"
#include "hybridpointertaskidentityworkflow.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QThread>
#include <QVariantMap>
#include <QWindow>

#include <functional>
#include <optional>

namespace QindaQt::Test {
namespace {

constexpr auto CompositorService = "org.qindaqt.Compositor";
constexpr auto KWinService = "org.kde.KWin";
constexpr auto KWinPath = "/KWin";
constexpr auto KWinInterface = "org.kde.KWin";
constexpr auto PluginPath = "/Plugins";
constexpr auto PluginInterface = "org.kde.KWin.Plugins";
constexpr auto PluginId = "qindaqt_compositor";

bool await(const std::function<bool()> &condition,
           int timeoutMilliseconds = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (condition()) {
            return true;
        }
        QThread::msleep(10);
    }
    return false;
}

bool serviceIsRegistered(QLatin1StringView service)
{
    const auto reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(
        QString(service));
    return reply.isValid() && reply.value();
}

QVariant unwrapped(QVariant value)
{
    return value.metaType() == QMetaType::fromType<QDBusVariant>()
        ? value.value<QDBusVariant>().variant() : value;
}

std::optional<QVariantMap> coreWindowInfo(QDBusInterface &kwin,
                                          const QString &windowId,
                                          QString *error)
{
    const QDBusReply<QVariantMap> reply = kwin.call(
        QStringLiteral("getWindowInfo"), windowId);
    if (!reply.isValid()) {
        *error = QStringLiteral("KWin getWindowInfo failed: %1")
                     .arg(reply.error().message());
        return std::nullopt;
    }
    if (reply.value().isEmpty()) {
        *error = QStringLiteral("KWin no longer reports window '%1'").arg(windowId);
        return std::nullopt;
    }
    return reply.value();
}

QRectF coreFrame(const QVariantMap &window)
{
    return {unwrapped(window.value(QStringLiteral("x"))).toDouble(),
            unwrapped(window.value(QStringLiteral("y"))).toDouble(),
            unwrapped(window.value(QStringLiteral("width"))).toDouble(),
            unwrapped(window.value(QStringLiteral("height"))).toDouble()};
}

QString coreCaption(const QVariantMap &window)
{
    return unwrapped(window.value(QStringLiteral("caption"))).toString();
}

bool coreMinimized(const QVariantMap &window)
{
    return unwrapped(window.value(QStringLiteral("minimized"))).toBool();
}

bool coreBoolean(const QVariantMap &window,
                 QLatin1StringView key,
                 bool *value,
                 QString *error)
{
    const QString field(key);
    if (!window.contains(field)) {
        *error = QStringLiteral("KWin getWindowInfo omitted '%1'").arg(field);
        return false;
    }
    *value = unwrapped(window.value(field)).toBool();
    return true;
}

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

QWindow *windowByTitle(const QString &title,
                       QWindow &primary,
                       QWindow &secondary,
                       QWindow &page)
{
    for (auto *candidate : {&primary, &secondary, &page}) {
        if (candidate->title() == title) {
            return candidate;
        }
    }
    return nullptr;
}

bool observedFramesAndVisibilityRestored(QDBusInterface &kwin,
                                         const HybridPointerGroupedState &state,
                                         QString *error)
{
    for (const auto &title : {state.gesture.sourceTitle,
                              state.gesture.targetTitle,
                              state.bystander}) {
        const auto &initial = window(state.initial, title);
        const auto current = coreWindowInfo(kwin, initial.id, error);
        bool skipTaskbar = false;
        bool skipSwitcher = false;
        if (!current
            || !coreBoolean(*current, QLatin1StringView("skipTaskbar"),
                            &skipTaskbar, error)
            || !coreBoolean(*current, QLatin1StringView("skipSwitcher"),
                            &skipSwitcher, error)
            || coreMinimized(*current) != initial.minimized
            || skipTaskbar != initial.skipTaskbar
            || skipSwitcher != initial.skipSwitcher
            || !sameGeometry(coreFrame(*current), initial.frame)) {
            return false;
        }
    }
    return true;
}

QJsonArray decorationClasses(const WindowInventory &inventory,
                             QString *error)
{
    QJsonArray result;
    for (const auto &observed : inventory) {
        if (!observed.serverDecorated
            || !observed.decorationClass.contains(QStringLiteral("QindaDecoration"))) {
            *error = QStringLiteral(
                "Hybrid unload probe did not start with runtime QindaQt decorations");
            return {};
        }
        result.append(observed.decorationClass);
    }
    return result;
}

} // namespace

PluginUnloadResult exerciseHybridPointerPluginUnload(
    QWindow &primary,
    QWindow &secondary,
    QWindow &page,
    QWindow &bystander,
    const QString &dotoolPath)
{
    PluginUnloadResult result;
    if (!await([] {
            return serviceIsRegistered(QLatin1StringView(CompositorService))
                && serviceIsRegistered(QLatin1StringView(KWinService));
        }, 5000)) {
        result.failure = QStringLiteral("required compositor/KWin services did not start");
        return result;
    }

    CompositorProbeClient client;
    QString error;
    const ProbeWindowTitles titles{primary.title(), secondary.title(), page.title()};
    HybridPointerGrouping pointer(client, titles);
    const auto state = pointer.group(dotoolPath, &error);
    if (!state) {
        result.failure = error;
        return result;
    }
    const auto classes = decorationClasses(state->initial, &error);
    if (classes.size() != state->initial.size()) {
        result.failure = error;
        return result;
    }
    result.grouped = true;
    const auto taskLifecycle = exerciseHybridTaskIdentityLifecycle(
        primary, secondary, page, pointer, *state, client, &error);
    if (!taskLifecycle) {
        result.failure = error;
        return result;
    }

    QDBusInterface kwin(QString::fromLatin1(KWinService),
                        QString::fromLatin1(KWinPath),
                        QString::fromLatin1(KWinInterface));
    QDBusInterface plugins(QString::fromLatin1(KWinService),
                           QString::fromLatin1(PluginPath),
                           QString::fromLatin1(PluginInterface));
    if (!kwin.isValid() || !plugins.isValid()) {
        result.failure = QStringLiteral("KWin core/plugin D-Bus interfaces are unavailable");
        return result;
    }
    const auto unloadReply = plugins.call(QStringLiteral("UnloadPlugin"),
                                          QString::fromLatin1(PluginId));
    result.unloadCallSucceeded = unloadReply.type() != QDBusMessage::ErrorMessage;
    if (!result.unloadCallSucceeded) {
        result.failure = QStringLiteral("UnloadPlugin failed: %1")
                             .arg(unloadReply.errorMessage());
        return result;
    }
    result.serviceRemoved = await([] {
        return !serviceIsRegistered(QLatin1StringView(CompositorService));
    });
    result.pluginRemoved = !plugins.property("LoadedPlugins").toStringList().contains(
        QString::fromLatin1(PluginId));
    if (!result.serviceRemoved || !result.pluginRemoved) {
        result.failure = QStringLiteral("KWin did not finish unloading the QindaQt plugin");
        return result;
    }

    result.framesRestored = await([&] {
        error.clear();
        return observedFramesAndVisibilityRestored(kwin, *state, &error);
    });
    if (!result.framesRestored) {
        result.failure = error.isEmpty()
            ? QStringLiteral("Hybrid unload stranded grouped client state") : error;
        return result;
    }

    auto *const usableWindow = windowByTitle(state->gesture.sourceTitle,
                                             primary, secondary, page);
    if (!usableWindow) {
        result.failure = QStringLiteral("grouped source no longer maps to a live probe client");
        return result;
    }
    const auto &sourceInitial = window(state->initial, state->gesture.sourceTitle);
    const auto usableTitle = QStringLiteral("QindaQt Hybrid unload source usable");
    const QSize requestedSize(qRound(sourceInitial.frame.width()) + 31,
                              qRound(sourceInitial.frame.height()) + 19);
    usableWindow->setTitle(usableTitle);
    usableWindow->resize(requestedSize);
    result.clientsUsable = await([&] {
        error.clear();
        const auto source = coreWindowInfo(kwin, sourceInitial.id, &error);
        return source && primary.isExposed() && secondary.isExposed()
            && page.isExposed() && bystander.isExposed()
            && usableWindow->size() == requestedSize
            && coreCaption(*source) == usableTitle;
    });
    if (!result.clientsUsable) {
        result.failure = error.isEmpty()
            ? QStringLiteral("clients stopped responding after Hybrid unload") : error;
        return result;
    }

    result.evidence = pointer.inputEvidence();
    result.evidence.insert(QStringLiteral("workflow"),
                           QStringLiteral("hybrid-pointer-plugin-unload"));
    result.evidence.insert(QStringLiteral("ownershipAuthority"),
                           QStringLiteral("hybrid-process"));
    result.evidence.insert(QStringLiteral("legacyBridgeContainersUsed"), false);
    result.evidence.insert(QStringLiteral("exactModifierGesture"),
                           QStringLiteral("Meta+Shift+Left"));
    result.evidence.insert(QStringLiteral("containerId"),
                           state->publicContainer.containerId);
    result.evidence.insert(QStringLiteral("publicContainerRevision"),
                           state->publicContainer.revision);
    result.evidence.insert(QStringLiteral("publicSnapshot"),
                           state->publicContainer.snapshot);
    result.evidence.insert(QStringLiteral("validTargetSplit"), state->split.valid);
    result.evidence.insert(QStringLiteral("splitOrientation"), state->split.orientation);
    result.evidence.insert(QStringLiteral("dividerGap"), state->split.dividerGap);
    result.evidence.insert(QStringLiteral("sourceRestoreFrame"),
                           rectJson(sourceInitial.frame));
    result.evidence.insert(
        QStringLiteral("targetRestoreFrame"),
        rectJson(window(state->initial, state->gesture.targetTitle).frame));
    // KWin's core API remains available after the plugin service disappears,
    // so these fields are independent evidence that shutdown recovery restored
    // the client-facing presentation baseline. Full WindowRestoreState equality
    // remains covered by the rollback-focused scene tests.
    result.evidence.insert(
        QStringLiteral("observedFramesAndVisibilityRestoredAfterUnload"), true);
    result.evidence.insert(
        QStringLiteral("observedIndependentStateRestoredAfterUnload"), true);
    result.evidence.insert(QStringLiteral("onePrimaryTaskIdentity"), true);
    result.evidence.insert(QStringLiteral("onePrimarySwitcherIdentity"), true);
    result.evidence.insert(
        QStringLiteral("samePrimaryTaskAndSwitcherIdentity"), true);
    result.evidence.insert(QStringLiteral("inactivePageExcluded"), true);
    result.evidence.insert(
        QStringLiteral("inactiveActivationActivatedPage"), true);
    result.evidence.insert(
        QStringLiteral("pageSwitchBackRetainsSingleIdentity"), true);
    result.evidence.insert(
        QStringLiteral("nativeMemberMinimizedWholeContainer"), true);
    result.evidence.insert(QStringLiteral("activePageOnlyRestore"), true);
    result.evidence.insert(
        QStringLiteral("taskFlagsRestoredAfterUnload"), true);
    result.evidence.insert(QStringLiteral("regroupedRevision"),
                           double(taskLifecycle->regroupedRevision));
    result.evidence.insert(QStringLiteral("inactivePageRevision"),
                           double(taskLifecycle->inactivePageRevision));
    result.evidence.insert(QStringLiteral("reactivatedSplitRevision"),
                           double(taskLifecycle->reactivatedSplitRevision));
    result.evidence.insert(QStringLiteral("activeSplitPageId"),
                           taskLifecycle->activeSplitPageId);
    result.evidence.insert(QStringLiteral("ownershipAuthorityRemoved"), true);
    result.evidence.insert(QStringLiteral("runtimeDecorationClasses"), classes);
    return result;
}

} // namespace QindaQt::Test
