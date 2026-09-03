// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitypointer.h"

#include "qindaqt/shell_orchestration/panel_interaction_store.h"
#include "qindaqt/shell_surface/layer_shell_surface_backend.h"
#include "qindaqt/shell_surface/panel_surface_controller.h"
#include "qindaqt/shell_surface/panel_window_factory.h"

#include <QEvent>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSet>
#include <QVariant>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace QindaQt::Shell {
namespace {

using Identity = ShellVisibility::PanelSurfaceIdentity;
using Lease = ShellOrchestration::PanelInteractionLease;

constexpr auto SensorPrefix = "qindaqt-panelvisibility-edge-";
constexpr auto PanelPrefix = "qindaqt-panel-";

QString identityKey(const Identity &identity)
{
    return identity.outputId + QChar(0x1f) + identity.panelId;
}

std::optional<Identity> identityFromWindow(const QObject &object)
{
    const QVariant panelProperty = object.property("qindaqtPanelVisibilityPanelId");
    const QVariant outputProperty = object.property("qindaqtPanelVisibilityOutputId");
    if (panelProperty.isValid() && outputProperty.isValid()) {
        return Identity{panelProperty.toString(), outputProperty.toString()};
    }
    const QString name = object.objectName();
    if (!name.startsWith(QLatin1StringView(PanelPrefix))
        || name.startsWith(QLatin1StringView(SensorPrefix))) {
        return std::nullopt;
    }
    const QString surface = name.mid(static_cast<qsizetype>(std::char_traits<char>::length(PanelPrefix)));
    const qsizetype separator = surface.lastIndexOf(QLatin1Char('@'));
    if (separator <= 0 || separator == surface.size() - 1) {
        return std::nullopt;
    }
    return Identity{surface.left(separator), surface.mid(separator + 1)};
}

bool containsIdentity(const QVector<Identity> &identities, const Identity &identity)
{
    return std::find(identities.cbegin(), identities.cend(), identity)
        != identities.cend();
}

Identity visibilityIdentity(const ShellSurface::PanelSurfaceIdentity &identity)
{
    return {identity.panelId, identity.outputId};
}

ShellSurface::PanelSurfaceConfiguration edgeConfiguration(
    const ShellSurface::PanelSurfaceConfiguration &panel)
{
    auto edge = panel;
    edge.identity.panelId = QStringLiteral("visibility-edge-%1")
                                .arg(panel.identity.panelId);
    edge.layer = Profiles::Layer::Overlay;
    edge.mapping = ShellSurface::PanelSurfaceMapping::Mapped;
    edge.reservesWorkArea = false;
    edge.reservationCarrier = false;
    edge.exclusiveZone = -1;
    edge.placementOrder = 0;
    QRect geometry = panel.geometry;
    QMargins margins = panel.margins;
    QSize desired = panel.desiredSize;
    switch (panel.edge) {
    case Profiles::Edge::Top:
        geometry.setY(panel.outputGeometry.top());
        geometry.setHeight(1);
        desired.setHeight(1);
        margins.setTop(0);
        break;
    case Profiles::Edge::Bottom:
        geometry.setY(panel.outputGeometry.bottom());
        geometry.setHeight(1);
        desired.setHeight(1);
        margins.setBottom(0);
        break;
    case Profiles::Edge::Left:
        geometry.setX(panel.outputGeometry.left());
        geometry.setWidth(1);
        desired.setWidth(1);
        margins.setLeft(0);
        break;
    case Profiles::Edge::Right:
        geometry.setX(panel.outputGeometry.right());
        geometry.setWidth(1);
        desired.setWidth(1);
        margins.setRight(0);
        break;
    }
    edge.geometry = geometry;
    edge.desiredSize = desired;
    edge.margins = margins;
    return edge;
}

} // namespace

class PanelVisibilityPointerProducer::Private final {
public:
    struct Entry {
        Identity identity;
        std::optional<Lease> reveal;
        quint64 pendingRelease = 0;
    };

    QGuiApplication &application;
    ShellOrchestration::PanelInteractionStore &interactions;
    PanelVisibilityTimerPort &timer;
    QVector<Identity> identities;
    std::map<QString, Entry> entries;
    int leaveDelayMilliseconds = 250;
};

PanelVisibilityPointerProducer::PanelVisibilityPointerProducer(
    QGuiApplication &application,
    ShellOrchestration::PanelInteractionStore &interactions,
    PanelVisibilityTimerPort &timer, QObject *parent)
    : QObject(parent)
    , m_private(new Private{application, interactions, timer, {}, {}, 250})
{
    application.installEventFilter(this);
}

PanelVisibilityPointerProducer::~PanelVisibilityPointerProducer()
{
    m_private->application.removeEventFilter(this);
    for (auto &[key, entry] : m_private->entries) {
        Q_UNUSED(key)
        m_private->timer.cancel(entry.pendingRelease);
    }
    delete m_private;
}

void PanelVisibilityPointerProducer::setIdentities(QVector<Identity> identities)
{
    m_private->identities = std::move(identities);
    for (auto item = m_private->entries.begin(); item != m_private->entries.end();) {
        if (!containsIdentity(m_private->identities, item->second.identity)) {
            m_private->timer.cancel(item->second.pendingRelease);
            item = m_private->entries.erase(item);
        } else {
            ++item;
        }
    }
}

void PanelVisibilityPointerProducer::setLeaveDelayMilliseconds(int delayMilliseconds)
{
    if (delayMilliseconds >= 0 && delayMilliseconds <= 5'000) {
        m_private->leaveDelayMilliseconds = delayMilliseconds;
    }
}

void PanelVisibilityPointerProducer::pointerEntered(const Identity &identity)
{
    if (!containsIdentity(m_private->identities, identity)) {
        return;
    }
    auto [item, inserted] = m_private->entries.try_emplace(
        identityKey(identity), Private::Entry{identity, std::nullopt, 0});
    Q_UNUSED(inserted)
    auto &entry = item->second;
    m_private->timer.cancel(entry.pendingRelease);
    entry.pendingRelease = 0;
    if (entry.reveal && entry.reveal->valid()) {
        return;
    }
    QString error;
    entry.reveal = m_private->interactions.acquire(
        identity, ShellOrchestration::PanelInteractionKind::Reveal, &error);
}

void PanelVisibilityPointerProducer::pointerLeft(const Identity &identity)
{
    const auto item = m_private->entries.find(identityKey(identity));
    if (item == m_private->entries.end()) {
        return;
    }
    auto &entry = item->second;
    m_private->timer.cancel(entry.pendingRelease);
    entry.pendingRelease = m_private->timer.schedule(
        m_private->leaveDelayMilliseconds, [this, key = identityKey(identity)] {
            const auto current = m_private->entries.find(key);
            if (current == m_private->entries.end()) {
                return;
            }
            current->second.pendingRelease = 0;
            current->second.reveal.reset();
        });
}

bool PanelVisibilityPointerProducer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != nullptr && event != nullptr) {
        const auto identity = identityFromWindow(*watched);
        if (identity && event->type() == QEvent::Enter) {
            pointerEntered(*identity);
        } else if (identity && (event->type() == QEvent::Leave
                               || event->type() == QEvent::Hide
                               || event->type() == QEvent::Close)) {
            pointerLeft(*identity);
        }
    }
    return QObject::eventFilter(watched, event);
}

class EdgeWindowFactory final : public ShellSurface::PanelWindowFactory {
public:
    explicit EdgeWindowFactory(PanelVisibilityPointerProducer &pointer)
        : m_pointer(pointer)
    {
    }

    std::unique_ptr<QQuickWindow> createWindow(
        const ShellSurface::PanelSurfaceConfiguration &configuration,
        QString *error) override
    {
        auto window = std::make_unique<QQuickWindow>();
        const QString panelId = configuration.identity.panelId.mid(
            QStringLiteral("visibility-edge-").size());
        window->setObjectName(QStringLiteral("%1%2@%3")
                                  .arg(QLatin1StringView(SensorPrefix), panelId,
                                       configuration.identity.outputId));
        window->setProperty("qindaqtPanelVisibilityPanelId", panelId);
        window->setProperty("qindaqtPanelVisibilityOutputId",
                            configuration.identity.outputId);
        window->setColor(Qt::transparent);
        window->setFlag(Qt::FramelessWindowHint, true);
        window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
        if (error != nullptr) {
            error->clear();
        }
        Q_UNUSED(m_pointer)
        return window;
    }

private:
    PanelVisibilityPointerProducer &m_pointer;
};

class PanelVisibilityEdgeSurfaceProducer::Private final {
public:
    explicit Private(PanelVisibilityPointerProducer &pointer)
        : factory(pointer)
        , backend(factory)
        , controller(backend)
    {
    }

    EdgeWindowFactory factory;
    ShellSurface::LayerShellSurfaceBackend backend;
    ShellSurface::PanelSurfaceController controller;
};

PanelVisibilityEdgeSurfaceProducer::PanelVisibilityEdgeSurfaceProducer(
    PanelVisibilityPointerProducer &pointer)
    : m_private(new Private(pointer))
{
}

PanelVisibilityEdgeSurfaceProducer::~PanelVisibilityEdgeSurfaceProducer()
{
    delete m_private;
}

bool PanelVisibilityEdgeSurfaceProducer::synchronize(
    const ShellSurface::PanelSurfacePlan &panelPlan,
    const QVector<Identity> &hideable, QString *error)
{
    ShellSurface::PanelSurfacePlan sensors;
    for (const auto &panel : panelPlan.surfaces) {
        if (containsIdentity(hideable, visibilityIdentity(panel.identity))) {
            sensors.surfaces.append(edgeConfiguration(panel));
        }
    }
    const auto result = m_private->controller.reconcilePlan(std::move(sensors));
    if (!result.ok()) {
        if (error != nullptr) {
            *error = result.message;
        }
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace QindaQt::Shell
