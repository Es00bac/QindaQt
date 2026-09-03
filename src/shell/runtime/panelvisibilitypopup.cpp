// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitypopup.h"

#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QMetaProperty>
#include <QPointer>
#include <QScreen>
#include <QWindow>

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
    QGuiApplication &application;
    ShellOrchestration::PanelInteractionStore &interactions;
    QVector<Identity> identities;
    std::map<QString, std::vector<Lease>> leases;
    QHash<QObject *, QString> popupOutputs;
};

PanelVisibilityPopupProducer::PanelVisibilityPopupProducer(
    QGuiApplication &application,
    ShellOrchestration::PanelInteractionStore &interactions, QObject *parent)
    : QObject(parent)
    , m_private(new Private{application, interactions, {}, {}, {}})
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
                setPopupVisible(QStringLiteral("object:%1")
                                    .arg(reinterpret_cast<quintptr>(object), 0, 16),
                                {}, false);
                m_private->popupOutputs.remove(object);
            });
            if (object->property("visible").toBool()) {
                setPopupVisible(
                    QStringLiteral("object:%1")
                        .arg(reinterpret_cast<quintptr>(object), 0, 16),
                    outputName(*window), true);
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
    setPopupVisible(
        QStringLiteral("object:%1")
            .arg(reinterpret_cast<quintptr>(object), 0, 16),
        m_private->popupOutputs.value(object),
        object->property("visible").toBool());
}

PanelVisibilityPopupProducer::~PanelVisibilityPopupProducer()
{
    m_private->application.removeEventFilter(this);
    delete m_private;
}

void PanelVisibilityPopupProducer::setIdentities(QVector<Identity> identities)
{
    if (m_private->identities == identities) {
        return;
    }
    // Identity replacement invalidates leases in the store. Reacquiring on a
    // later Show event is safer than carrying a popup across output topology.
    m_private->leases.clear();
    m_private->identities = std::move(identities);
}

void PanelVisibilityPopupProducer::setPopupVisible(
    const QString &sourceId, const QString &outputId, bool visible)
{
    if (sourceId.trimmed().isEmpty()) {
        return;
    }
    if (!visible) {
        m_private->leases.erase(sourceId);
        return;
    }
    if (m_private->leases.contains(sourceId)) {
        return;
    }
    std::vector<Lease> acquired;
    for (const Identity &identity : std::as_const(m_private->identities)) {
        if (!outputId.isEmpty() && identity.outputId != outputId) {
            continue;
        }
        QString error;
        auto lease = m_private->interactions.acquire(
            identity, ShellOrchestration::PanelInteractionKind::VisibilityHold,
            &error);
        if (lease) {
            acquired.push_back(std::move(*lease));
        }
    }
    if (!acquired.empty()) {
        m_private->leases.emplace(sourceId, std::move(acquired));
    }
}

bool PanelVisibilityPopupProducer::eventFilter(QObject *watched, QEvent *event)
{
    auto *const window = qobject_cast<QWindow *>(watched);
    if (window == nullptr || event == nullptr || !isShellPopup(*window)) {
        return QObject::eventFilter(watched, event);
    }
    const QString source = windowSource(*window);
    if (event->type() == QEvent::Show) {
        setPopupVisible(source, outputName(*window), true);
    } else if (event->type() == QEvent::Hide || event->type() == QEvent::Close
               || event->type() == QEvent::Destroy) {
        setPopupVisible(source, {}, false);
    }
    return QObject::eventFilter(watched, event);
}

} // namespace QindaQt::Shell
