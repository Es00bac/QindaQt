// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/layer_shell_surface_backend.h"

#include "qindaqt/shell_surface/panel_window_factory.h"
#include "qindaqt/shell_surface/qt_output_inventory.h"

#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QHash>
#include <QQuickWindow>
#include <QScreen>
#include <QSet>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace QindaQt::ShellSurface {
namespace {

struct StagedWindow {
    PanelSurfaceConfiguration configuration;
    std::unique_ptr<QQuickWindow> window;
};

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

std::optional<LayerShellQt::Window::Layer> protocolLayer(Profiles::Layer layer)
{
    switch (layer) {
    case Profiles::Layer::Below:
        return LayerShellQt::Window::LayerBottom;
    case Profiles::Layer::Normal:
        // AGENT-NOTE: zwlr-layer-shell has no ordinary-application layer.
        // Schema-v1 Normal and Above both reserve application work area, so the
        // current backend maps both to Top while retaining the original enum in
        // PanelSurfaceConfiguration for a future compositor-native distinction.
        return LayerShellQt::Window::LayerTop;
    case Profiles::Layer::Above:
        return LayerShellQt::Window::LayerTop;
    case Profiles::Layer::Overlay:
        return LayerShellQt::Window::LayerOverlay;
    }
    return std::nullopt;
}

LayerShellQt::Window::Anchor protocolEdge(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
        return LayerShellQt::Window::AnchorTop;
    case Profiles::Edge::Bottom:
        return LayerShellQt::Window::AnchorBottom;
    case Profiles::Edge::Left:
        return LayerShellQt::Window::AnchorLeft;
    case Profiles::Edge::Right:
        return LayerShellQt::Window::AnchorRight;
    }
    return LayerShellQt::Window::AnchorNone;
}

SurfaceAnchor surfaceEdgeAnchor(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
        return SurfaceAnchor::Top;
    case Profiles::Edge::Bottom:
        return SurfaceAnchor::Bottom;
    case Profiles::Edge::Left:
        return SurfaceAnchor::Left;
    case Profiles::Edge::Right:
        return SurfaceAnchor::Right;
    }
    return static_cast<SurfaceAnchor>(0);
}

bool validEdge(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
    case Profiles::Edge::Bottom:
    case Profiles::Edge::Left:
    case Profiles::Edge::Right:
        return true;
    }
    return false;
}

LayerShellQt::Window::Anchors protocolAnchors(SurfaceAnchors anchors)
{
    LayerShellQt::Window::Anchors result;
    if (anchors.testFlag(SurfaceAnchor::Top)) {
        result |= LayerShellQt::Window::AnchorTop;
    }
    if (anchors.testFlag(SurfaceAnchor::Bottom)) {
        result |= LayerShellQt::Window::AnchorBottom;
    }
    if (anchors.testFlag(SurfaceAnchor::Left)) {
        result |= LayerShellQt::Window::AnchorLeft;
    }
    if (anchors.testFlag(SurfaceAnchor::Right)) {
        result |= LayerShellQt::Window::AnchorRight;
    }
    return result;
}

bool nonNegativeMargins(const QMargins &margins)
{
    return margins.left() >= 0 && margins.top() >= 0 && margins.right() >= 0 &&
        margins.bottom() >= 0;
}

bool validDesiredSize(const PanelSurfaceConfiguration &configuration)
{
    if (configuration.desiredSize.width() < 0 || configuration.desiredSize.height() < 0) {
        return false;
    }
    if (configuration.desiredSize.width() == 0 &&
        !(configuration.anchors.testFlag(SurfaceAnchor::Left) &&
          configuration.anchors.testFlag(SurfaceAnchor::Right))) {
        return false;
    }
    if (configuration.desiredSize.height() == 0 &&
        !(configuration.anchors.testFlag(SurfaceAnchor::Top) &&
          configuration.anchors.testFlag(SurfaceAnchor::Bottom))) {
        return false;
    }
    return configuration.desiredSize.width() != 0 || configuration.desiredSize.height() != 0;
}

bool validateConfiguration(const PanelSurfaceConfiguration &configuration, QString *error)
{
    if (configuration.identity.panelId.trimmed().isEmpty() ||
        configuration.identity.outputId.trimmed().isEmpty()) {
        setError(error, QStringLiteral("a panel surface identity is empty"));
        return false;
    }
    if (!validEdge(configuration.edge) || !validEdge(configuration.exclusiveEdge) ||
        configuration.edge != configuration.exclusiveEdge) {
        setError(error,
                 QStringLiteral("panel '%1' has an unsupported edge")
                     .arg(configuration.identity.panelId));
        return false;
    }
    if (!configuration.geometry.isValid() || !nonNegativeMargins(configuration.margins) ||
        !validDesiredSize(configuration)) {
        setError(error,
                 QStringLiteral("panel '%1' has invalid protocol geometry")
                     .arg(configuration.identity.panelId));
        return false;
    }
    if (configuration.exclusiveZone == 0 || configuration.exclusiveZone < -1 ||
        configuration.reservationCarrier != (configuration.exclusiveZone > 0) ||
        !configuration.anchors.testFlag(surfaceEdgeAnchor(configuration.edge))) {
        setError(error,
                 QStringLiteral("panel '%1' has invalid exclusive-zone values")
                     .arg(configuration.identity.panelId));
        return false;
    }
    if (!protocolLayer(configuration.layer)) {
        setError(error,
                 QStringLiteral("panel '%1' has an unsupported layer")
                     .arg(configuration.identity.panelId));
        return false;
    }
    return true;
}

class PublishedLayerShellSurfaceSet final : public PublishedSurfaceSet {
public:
    explicit PublishedLayerShellSurfaceSet(std::vector<StagedWindow> windows)
        : m_windows(std::move(windows))
    {
    }

    ~PublishedLayerShellSurfaceSet() override
    {
        // Hide the complete set before QObject destruction starts so KWin
        // receives role unmaps as one synchronous controller teardown phase.
        for (auto &entry : m_windows) {
            entry.window->hide();
        }
    }

    bool isLive() const noexcept override
    {
        return std::all_of(m_windows.cbegin(), m_windows.cend(), [](const auto &entry) {
            // LayerShellQt closes its QWindow after a compositor dismissal.
            // The retained object is then intentionally treated as stale so
            // an identical returned output/layout creates a fresh role.
            return entry.window != nullptr && entry.window->isVisible();
        });
    }

private:
    std::vector<StagedWindow> m_windows;
};

class PreparedLayerShellSurfaceSet final : public PreparedSurfaceSet {
public:
    explicit PreparedLayerShellSurfaceSet(std::vector<StagedWindow> windows)
        : m_windows(std::move(windows))
    {
    }

    std::unique_ptr<PublishedSurfaceSet> publish(QString *error) override
    {
        if (m_published) {
            setError(error, QStringLiteral("the prepared panel set was already published"));
            return nullptr;
        }

        for (auto &entry : m_windows) {
            entry.window->show();
            if (!entry.window->isVisible()) {
                for (auto &rollback : m_windows) {
                    rollback.window->hide();
                }
                setError(error,
                         QStringLiteral("panel '%1' could not be made visible")
                             .arg(entry.configuration.identity.panelId));
                return nullptr;
            }
        }

        // AGENT-CONTRACT: Wayland maps/configures asynchronously and offers no
        // multi-surface transaction. This boundary guarantees synchronous Qt
        // staging failure atomicity; compositor acknowledgement belongs to the
        // nested runtime qualification rather than being guessed here.
        m_published = true;
        return std::make_unique<PublishedLayerShellSurfaceSet>(std::move(m_windows));
    }

private:
    std::vector<StagedWindow> m_windows;
    bool m_published = false;
};

} // namespace

LayerShellSurfaceBackend::LayerShellSurfaceBackend(PanelWindowFactory &factory)
    : m_factory(factory)
{
}

LayerShellSurfaceBackend::~LayerShellSurfaceBackend() = default;

std::unique_ptr<PreparedSurfaceSet> LayerShellSurfaceBackend::prepare(
    const QVector<PanelSurfaceConfiguration> &configurations, QString *error)
{
    if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland"))) {
        setError(error, QStringLiteral("real panel surfaces require Qt's Wayland platform"));
        return nullptr;
    }

    QVector<PanelSurfaceConfiguration> ordered = configurations;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto &left, const auto &right) {
                         return left.placementOrder < right.placementOrder;
                     });

    std::vector<StagedWindow> staged;
    staged.reserve(static_cast<std::size_t>(ordered.size()));
    QHash<QString, QSet<QString>> panelIdsByOutput;
    for (const auto &configuration : std::as_const(ordered)) {
        if (!validateConfiguration(configuration, error)) {
            return nullptr;
        }
        auto &panelIds = panelIdsByOutput[configuration.identity.outputId];
        if (panelIds.contains(configuration.identity.panelId)) {
            setError(error,
                     QStringLiteral("duplicate panel '%1' on output '%2'")
                         .arg(configuration.identity.panelId, configuration.identity.outputId));
            return nullptr;
        }
        panelIds.insert(configuration.identity.panelId);

        QString diagnostic;
        QScreen *screen = QtOutputInventory::screenForId(configuration.identity.outputId,
                                                         &diagnostic);
        if (screen == nullptr) {
            setError(error, std::move(diagnostic));
            return nullptr;
        }
        auto window = m_factory.createWindow(configuration, &diagnostic);
        if (!window) {
            setError(error,
                     diagnostic.trimmed().isEmpty()
                         ? QStringLiteral("panel window factory rejected '%1'")
                               .arg(configuration.identity.panelId)
                         : std::move(diagnostic));
            return nullptr;
        }
        if (window->isVisible() || window->parent() != nullptr ||
            window->transientParent() != nullptr) {
            setError(error,
                     QStringLiteral("panel window '%1' must be hidden and unparented")
                         .arg(configuration.identity.panelId));
            return nullptr;
        }

        window->setFlag(Qt::FramelessWindowHint, true);
        window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
        window->setColor(Qt::transparent);
        window->setScreen(screen);
        window->resize(configuration.geometry.size());

        auto *layerWindow = LayerShellQt::Window::get(window.get());
        if (layerWindow == nullptr) {
            setError(error,
                     QStringLiteral("LayerShellQt rejected panel '%1'")
                         .arg(configuration.identity.panelId));
            return nullptr;
        }
        layerWindow->setWantsToBeOnActiveScreen(false);
        layerWindow->setScreen(screen);
        layerWindow->setScope(QStringLiteral("dock"));
        layerWindow->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivityNone);
        layerWindow->setActivateOnShow(false);
        layerWindow->setCloseOnDismissed(true);
        layerWindow->setAnchors(protocolAnchors(configuration.anchors));
        layerWindow->setMargins(configuration.margins);
        layerWindow->setDesiredSize(configuration.desiredSize);
        layerWindow->setLayer(*protocolLayer(configuration.layer));
        layerWindow->setExclusiveEdge(protocolEdge(configuration.exclusiveEdge));
        layerWindow->setExclusiveZone(configuration.exclusiveZone);
        staged.push_back({configuration, std::move(window)});
    }
    return std::make_unique<PreparedLayerShellSurfaceSet>(std::move(staged));
}

} // namespace QindaQt::ShellSurface
