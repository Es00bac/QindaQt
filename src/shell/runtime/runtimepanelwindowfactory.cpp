// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtimepanelwindowfactory.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStringList>

#include <utility>

namespace QindaQt::Shell {
namespace {

QString componentErrors(const QQmlComponent &component)
{
    QStringList messages;
    const auto errors = component.errors();
    messages.reserve(errors.size());
    for (const auto &qmlError : errors) {
        messages.push_back(qmlError.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

} // namespace

RuntimePanelWindowFactory::RuntimePanelWindowFactory(QQmlEngine &engine,
                                                     const Profiles::LayoutProfile &profile,
                                                     QVariantMap theme)
    : m_engine(engine)
    , m_theme(std::move(theme))
{
    for (const auto &panel : profile.panels) {
        m_panels.insert(panel.id, panel.toVariantMap());
    }
}

RuntimePanelWindowFactory::~RuntimePanelWindowFactory() = default;

bool RuntimePanelWindowFactory::ensureComponent(QString *error)
{
    if (m_component && m_component->isReady()) {
        return true;
    }
    if (!m_component) {
        m_component = std::make_unique<QQmlComponent>(&m_engine);
        m_component->loadFromModule(QStringLiteral("QindaQt.Shell.Runtime"),
                                    QStringLiteral("RuntimePanel"));
    }
    if (!m_component->isReady()) {
        if (error != nullptr) {
            *error = QStringLiteral("cannot load the runtime panel QML component: %1")
                         .arg(componentErrors(*m_component));
        }
        return false;
    }
    return true;
}

std::unique_ptr<QQuickWindow> RuntimePanelWindowFactory::createWindow(
    const ShellSurface::PanelSurfaceConfiguration &configuration, QString *error)
{
    const auto panel = m_panels.constFind(configuration.identity.panelId);
    if (panel == m_panels.cend()) {
        if (error != nullptr) {
            *error = QStringLiteral("surface '%1' on '%2' has no selected-profile panel")
                         .arg(configuration.identity.panelId, configuration.identity.outputId);
        }
        return {};
    }
    if (!ensureComponent(error)) {
        return {};
    }

    const QString surfaceId = QStringLiteral("%1@%2")
                                  .arg(configuration.identity.panelId,
                                       configuration.identity.outputId);
    const QVariantMap initialProperties = {
        {QStringLiteral("panel"), panel.value()},
        {QStringLiteral("theme"), m_theme},
        {QStringLiteral("surfaceId"), surfaceId},
    };
    QObject *created = m_component->createWithInitialProperties(initialProperties);
    auto *window = qobject_cast<QQuickWindow *>(created);
    if (window == nullptr) {
        delete created;
        if (error != nullptr) {
            const QString details = componentErrors(*m_component);
            *error = details.isEmpty()
                ? QStringLiteral("runtime panel component did not create a QQuickWindow")
                : QStringLiteral("runtime panel creation failed: %1").arg(details);
        }
        return {};
    }

    // AGENT-CONTRACT: LayerShellSurfaceBackend must assign every Wayland role
    // property before the first map. QML therefore keeps this window hidden;
    // showing it here can permanently turn it into an ordinary toplevel.
    if (window->isVisible()) {
        window->hide();
    }
    window->setObjectName(QStringLiteral("qindaqt-panel-%1").arg(surfaceId));
    return std::unique_ptr<QQuickWindow>(window);
}

} // namespace QindaQt::Shell
