// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridcompositorrestart.h"

#include "compositorprobeclient.h"
#include "hybridpointergrouping.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QThread>

namespace QindaQt::Test {
namespace {

constexpr int RestartTimeoutMilliseconds = 6000;

class CompositingTransitionObserver final : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] bool completedRestart() const noexcept
    {
        return m_sawInactive && m_sawActiveAfterInactive;
    }

    [[nodiscard]] QString diagnostics() const
    {
        QStringList states;
        states.reserve(m_transitions.size());
        for (const bool active : m_transitions) {
            states.append(active ? QStringLiteral("active")
                                 : QStringLiteral("inactive"));
        }
        return states.join(QLatin1Char(','));
    }

public Q_SLOTS:
    void compositingToggled(bool active)
    {
        m_transitions.append(active);
        if (!active) {
            m_sawInactive = true;
        } else if (m_sawInactive) {
            m_sawActiveAfterInactive = true;
        }
    }

private:
    QList<bool> m_transitions;
    bool m_sawInactive = false;
    bool m_sawActiveAfterInactive = false;
};

const ObservedWindow &window(const WindowInventory &inventory,
                             const QString &title)
{
    return inventory.constFind(title).value();
}

bool onlyContainer(const QJsonArray &containers, const QString &containerId,
                   const QString &revision)
{
    if (containers.size() != 1) {
        return false;
    }
    const auto container = containers.at(0).toObject();
    return container.value(QStringLiteral("id")).toString() == containerId
        && container.value(QStringLiteral("revision")).toString() == revision;
}

bool scenePublicationMatches(const QJsonObject &hybrid,
                             const QString &topologyRevision)
{
    return hybrid.value(QStringLiteral("ready")).toBool()
        && hybrid.value(QStringLiteral("topologyRevision")).toString()
            == topologyRevision
        && hybrid.value(QStringLiteral("containerCount")).toInt(-1) == 1
        && hybrid.value(QStringLiteral("chromeOverlayCount")).toInt(-1) == 1
        && hybrid.value(
               QStringLiteral("visibleAnchoredChromeSceneItemCount"))
               .toInt(-1) == 1;
}

bool groupedClientsMatch(const WindowInventory &inventory,
                         const HybridPointerGroupedState &state)
{
    const auto &source = window(inventory, state.gesture.sourceTitle);
    const auto &target = window(inventory, state.gesture.targetTitle);
    const auto &bystander = window(inventory, state.bystander);
    const auto &sourceBefore = window(state.grouped, state.gesture.sourceTitle);
    const auto &targetBefore = window(state.grouped, state.gesture.targetTitle);
    const auto &bystanderBefore = window(state.grouped, state.bystander);
    return source.containerId == state.publicContainer.containerId
        && target.containerId == state.publicContainer.containerId
        && sameGeometry(source.targetFrame, sourceBefore.targetFrame)
        && sameGeometry(target.targetFrame, targetBefore.targetFrame)
        && bystander.containerId.isEmpty() && !bystander.minimized
        && sameGeometry(bystander.frame, bystanderBefore.frame);
}

bool awaitInitialScenePublication(
    CompositorProbeClient &client,
    const HybridCompositorRestartEvidence &expected,
    const QString &containerId,
    QString *error)
{
    QElapsedTimer timer;
    timer.start();
    QJsonObject lastHybrid;
    QJsonArray lastContainers;
    while (timer.elapsed() < RestartTimeoutMilliseconds) {
        QString probeError;
        const auto capabilities = client.call(QStringLiteral("Capabilities"), &probeError);
        const auto containers = client.containers(&probeError);
        if (capabilities) {
            lastHybrid = capabilities->value(QStringLiteral("hybrid")).toObject();
        }
        if (containers) {
            lastContainers = *containers;
        }
        if (containers && containers->size() == 1) {
            const auto container = containers->at(0).toObject();
            if (container.value(QStringLiteral("id")).toString() == containerId
                && scenePublicationMatches(lastHybrid, expected.topologyRevision)
                && container.value(QStringLiteral("revision")).toString()
                    == expected.containerRevision) {
                error->clear();
                return true;
            }
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    *error = QStringLiteral(
        "compositor restart preflight did not expose one anchored group; "
        "hybrid=%1 containers=%2")
                 .arg(QString::fromUtf8(QJsonDocument(lastHybrid).toJson(
                          QJsonDocument::Compact)),
                      QString::fromUtf8(QJsonDocument(lastContainers).toJson(
                          QJsonDocument::Compact)));
    return false;
}

} // namespace

std::optional<HybridCompositorRestartEvidence>
exerciseHybridCompositorRestart(CompositorProbeClient &client,
                                const HybridPointerGroupedState &state,
                                QString *error)
{
    const HybridCompositorRestartEvidence evidence{
        .topologyRevision = QString::number(state.groupedHybrid.revision),
        .containerRevision = state.publicContainer.revision,
    };
    const auto &containerId = state.publicContainer.containerId;
    if (!awaitInitialScenePublication(client, evidence, containerId, error)) {
        return std::nullopt;
    }

    auto bus = QDBusConnection::sessionBus();
    CompositingTransitionObserver observer;
    if (!bus.connect(QStringLiteral("org.kde.KWin"),
                     QStringLiteral("/Compositor"),
                     QStringLiteral("org.kde.kwin.Compositing"),
                     QStringLiteral("compositingToggled"),
                     &observer, SLOT(compositingToggled(bool)))) {
        *error = QStringLiteral(
            "could not observe KWin's compositor restart transition");
        return std::nullopt;
    }

    // The development endpoint queues KWin::Compositor::reinitialize() so its
    // blocking D-Bus reply cannot race the compositor teardown it requested.
    // AGENT-GUARD: Keep the off->on observation too: `scheduled` proves only
    // admission, while the transitions prove the queued KWin work completed.
    const auto reinitialize = client.call(
        QStringLiteral("ReinitializeCompositingForTest"), error);
    if (!reinitialize
        || reinitialize->value(QStringLiteral("status"))
            != QStringLiteral("scheduled")) {
        if (error->isEmpty()) {
            *error = QStringLiteral(
                "compositor endpoint did not schedule reinitialization");
        }
        return std::nullopt;
    }

    QElapsedTimer timer;
    timer.start();
    QJsonObject lastHybrid;
    QJsonArray lastContainers;
    bool publicationRestored = false;
    while (timer.elapsed() < RestartTimeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QString probeError;
        const auto capabilities = client.call(QStringLiteral("Capabilities"), &probeError);
        const auto containers = client.containers(&probeError);
        if (capabilities) {
            lastHybrid = capabilities->value(QStringLiteral("hybrid")).toObject();
        }
        if (containers) {
            lastContainers = *containers;
        }
        if (observer.completedRestart() && capabilities && containers
            && scenePublicationMatches(lastHybrid, evidence.topologyRevision)
            && onlyContainer(*containers, containerId, evidence.containerRevision)) {
            publicationRestored = true;
            break;
        }
        QThread::msleep(10);
    }
    bus.disconnect(QStringLiteral("org.kde.KWin"),
                   QStringLiteral("/Compositor"),
                   QStringLiteral("org.kde.kwin.Compositing"),
                   QStringLiteral("compositingToggled"),
                   &observer, SLOT(compositingToggled(bool)));
    if (!publicationRestored) {
        *error = QStringLiteral(
            "compositor restart did not restore the same grouped scene publication; "
            "transitions=%1 hybrid=%2 containers=%3")
                     .arg(observer.diagnostics(),
                          QString::fromUtf8(QJsonDocument(lastHybrid).toJson(
                              QJsonDocument::Compact)),
                          QString::fromUtf8(QJsonDocument(lastContainers).toJson(
                              QJsonDocument::Compact)));
        return std::nullopt;
    }

    const auto regrouped = client.awaitWindows(
        {state.gesture.sourceTitle, state.gesture.targetTitle, state.bystander},
        [&](const WindowInventory &inventory) {
            return groupedClientsMatch(inventory, state);
        },
        error);
    if (!regrouped) {
        *error = QStringLiteral(
            "compositor restart changed grouped client realization: %1").arg(*error);
        return std::nullopt;
    }
    return evidence;
}

} // namespace QindaQt::Test

#include "hybridcompositorrestart.moc"
