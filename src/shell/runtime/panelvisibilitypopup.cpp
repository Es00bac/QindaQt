// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitypopup.h"

#include "panelvisibilitytimer.h"

#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QMetaProperty>
#include <QScreen>
#include <QWindow>

#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace QindaQt::Shell {
namespace {

using Identity = ShellVisibility::PanelSurfaceIdentity;
using Lease = ShellOrchestration::PanelInteractionLease;

bool isPanelWindow(const QWindow &window)
{
    return window.objectName().startsWith(QStringLiteral("qindaqt-panel-"))
        || window.objectName().startsWith(
            QStringLiteral("qindaqt-panelvisibility-edge-"));
}

bool isShellPopup(const QWindow &window)
{
    if (isPanelWindow(window)) {
        return false;
    }
    return window.objectName().startsWith(QStringLiteral("qindaqt-notification-"))
        || window.type() == Qt::Popup || window.transientParent() != nullptr;
}

QString windowSource(const QWindow &window)
{
    return QStringLiteral("window:%1")
        .arg(reinterpret_cast<quintptr>(&window), 0, 16);
}

QString outputName(const QWindow &window)
{
    const QScreen *screen = window.screen();
    if (screen == nullptr && window.transientParent() != nullptr) {
        screen = window.transientParent()->screen();
    }
    return screen != nullptr ? screen->name() : QString{};
}

} // namespace

class PanelVisibilityPopupProducer::Private final {
public:
    struct Source final {
        QObject *owner = nullptr;
        quint64 generation = 0;
        quint64 expiryToken = 0;
        QMetaObject::Connection ownerDestroyed;
        std::vector<Lease> leases;
    };

    QGuiApplication &application;
    ShellOrchestration::PanelInteractionStore &interactions;
    PanelVisibilityTimerPort &timer;
    QVector<Identity> identities;
    std::map<QString, Source> sources;
    QHash<QObject *, QString> popupOutputs;
    qsizetype leaseCount = 0;
    quint64 nextGeneration = 1;
};

PanelVisibilityPopupProducer::PanelVisibilityPopupProducer(
    QGuiApplication &application,
    ShellOrchestration::PanelInteractionStore &interactions,
    PanelVisibilityTimerPort &timer, QObject *parent)
    : QObject(parent)
    , m_private(new Private{application, interactions, timer, {}, {}, {}, 0, 1})
{
    application.installEventFilter(this);
}

void PanelVisibilityPopupProducer::synchronizePopupObjects()
{
    const auto windows = m_private->application.allWindows();
    for (QWindow *window : windows) {
        if (window == nullptr || isPanelWindow(*window) == false) {
            continue;
        }
        const auto objects = window->findChildren<QObject *>();
        for (QObject *object : objects) {
            if (object == nullptr || m_private->popupOutputs.contains(object)
                || !object->objectName().endsWith(
                    QLatin1StringView("Popup"))) {
                continue;
            }
            const int propertyIndex = object->metaObject()->indexOfProperty("visible");
            if (propertyIndex < 0
                || !object->metaObject()->property(propertyIndex).hasNotifySignal()) {
                continue;
            }
            const bool connected = connect(
                object, SIGNAL(visibleChanged()), this,
                SLOT(popupObjectVisibilityChanged()), Qt::UniqueConnection);
            if (!connected) {
                continue;
            }
            // AGENT-CONTRACT: QQuick Popup QObject ancestry is not its visual
            // window ancestry. Retain the panel output found during discovery
            // so a visible popup cannot accidentally hold every output.
            m_private->popupOutputs.insert(object, outputName(*window));
            connect(object, &QObject::destroyed, this, [this, object] {
                m_private->popupOutputs.remove(object);
            });
            if (object->property("visible").toBool()) {
                const bool admitted = setPopupVisible(
                    object,
                    QStringLiteral("object:%1")
                        .arg(reinterpret_cast<quintptr>(object), 0, 16),
                    outputName(*window), true);
                Q_UNUSED(admitted)
            }
        }
    }
}

void PanelVisibilityPopupProducer::popupObjectVisibilityChanged()
{
    QObject *const object = sender();
    if (object == nullptr) {
        return;
    }
    const bool admitted = setPopupVisible(
        object,
        QStringLiteral("object:%1")
            .arg(reinterpret_cast<quintptr>(object), 0, 16),
        m_private->popupOutputs.value(object),
        object->property("visible").toBool());
    Q_UNUSED(admitted)
}

PanelVisibilityPopupProducer::~PanelVisibilityPopupProducer()
{
    m_private->application.removeEventFilter(this);
    clearSources();
    delete m_private;
}

void PanelVisibilityPopupProducer::setIdentities(QVector<Identity> identities)
{
    if (m_private->identities == identities) {
        return;
    }
    // Identity replacement invalidates leases in the store. Reacquiring on a
    // later Show event is safer than carrying a popup across output topology.
    clearSources();
    m_private->identities = std::move(identities);
}

bool PanelVisibilityPopupProducer::setPopupVisible(
    QObject *owner, const QString &sourceId, const QString &outputId,
    bool visible)
{
    if (owner == nullptr || sourceId.size() > MaximumSourceIdLength
        || sourceId.trimmed().isEmpty()) {
        return false;
    }
    const auto existing = m_private->sources.find(sourceId);
    if (existing != m_private->sources.end()) {
        if (existing->second.owner != owner) {
            return false;
        }
        if (visible) {
            // AGENT-GUARD: Repeated visible notifications never extend the
            // maximum lifetime. A close/reopen transition is required.
            return true;
        }
        releaseSource(sourceId, existing->second.generation);
        return true;
    }
    if (!visible) {
        return true;
    }
    if (std::ssize(m_private->sources) >= MaximumSources
        || m_private->nextGeneration == 0) {
        return false;
    }
    QVector<Identity> selected;
    for (const Identity &identity : std::as_const(m_private->identities)) {
        if (outputId.isEmpty() || identity.outputId == outputId) {
            selected.append(identity);
        }
    }
    if (selected.isEmpty()
        || selected.size() > MaximumLeases - m_private->leaseCount) {
        return false;
    }
    std::vector<Lease> acquired;
    acquired.reserve(static_cast<std::size_t>(selected.size()));
    for (const Identity &identity : std::as_const(selected)) {
        QString error;
        auto lease = m_private->interactions.acquire(
            identity, ShellOrchestration::PanelInteractionKind::VisibilityHold,
            &error);
        if (lease) {
            acquired.push_back(std::move(*lease));
        } else {
            return false;
        }
    }
    const quint64 generation = m_private->nextGeneration;
    m_private->nextGeneration = generation == std::numeric_limits<quint64>::max()
        ? 0 : generation + 1;
    auto [iterator, inserted] = m_private->sources.emplace(
        sourceId, Private::Source{owner, generation, 0, {}, std::move(acquired)});
    if (!inserted) {
        return false;
    }
    iterator->second.ownerDestroyed = connect(
        owner, &QObject::destroyed, this,
        [this, sourceId, generation] { releaseSource(sourceId, generation); });
    iterator->second.expiryToken = m_private->timer.schedule(
        MaximumHoldMilliseconds,
        [this, sourceId, generation] { releaseSource(sourceId, generation); });
    if (iterator->second.expiryToken == 0) {
        QObject::disconnect(iterator->second.ownerDestroyed);
        m_private->sources.erase(iterator);
        return false;
    }
    m_private->leaseCount += selected.size();
    return true;
}

void PanelVisibilityPopupProducer::releaseSource(
    const QString &sourceId, quint64 generation)
{
    const auto iterator = m_private->sources.find(sourceId);
    if (iterator == m_private->sources.end()
        || iterator->second.generation != generation) {
        return;
    }
    m_private->timer.cancel(iterator->second.expiryToken);
    QObject::disconnect(iterator->second.ownerDestroyed);
    m_private->leaseCount -= std::ssize(iterator->second.leases);
    m_private->sources.erase(iterator);
}

void PanelVisibilityPopupProducer::clearSources()
{
    for (auto &[sourceId, source] : m_private->sources) {
        Q_UNUSED(sourceId)
        m_private->timer.cancel(source.expiryToken);
        QObject::disconnect(source.ownerDestroyed);
    }
    m_private->sources.clear();
    m_private->leaseCount = 0;
}

bool PanelVisibilityPopupProducer::eventFilter(QObject *watched, QEvent *event)
{
    auto *const window = qobject_cast<QWindow *>(watched);
    if (window == nullptr || event == nullptr || !isShellPopup(*window)) {
        return QObject::eventFilter(watched, event);
    }
    const QString source = windowSource(*window);
    if (event->type() == QEvent::Show) {
        const bool admitted = setPopupVisible(
            window, source, outputName(*window), true);
        Q_UNUSED(admitted)
    } else if (event->type() == QEvent::Hide || event->type() == QEvent::Close
               || event->type() == QEvent::Destroy) {
        const bool released = setPopupVisible(window, source, {}, false);
        Q_UNUSED(released)
    }
    return QObject::eventFilter(watched, event);
}

} // namespace QindaQt::Shell
