// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationwindowcontroller.h"
#include "settingsroutelauncher.h"

#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/shell_surface/layer_shell_notification_surface.h"
#include "qindaqt/shell_surface/notification_surface_layout.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QStringList>

#include <utility>

namespace QindaQt::Shell {
namespace {

constexpr QMargins PopupMargins{0, 16, 16, 0};
constexpr QMargins CenterMargins{0, 16, 16, 0};

QString componentErrors(const QQmlComponent &component)
{
    QStringList messages;
    const auto errors = component.errors();
    messages.reserve(errors.size());
    for (const auto &error : errors) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

NotificationWindowController::NotificationWindowController(
    QQmlEngine &engine,
    Services::NotificationPresentationModel::NotificationPresentationController &
        presentation,
    Services::SettingsClient::DoNotDisturbController &quietingSettings,
    SettingsRouteLauncher &settingsLauncher,
    QVariantMap theme)
    : m_engine(engine)
    , m_presentation(presentation)
    , m_quietingSettings(quietingSettings)
    , m_settingsLauncher(settingsLauncher)
    , m_theme(std::move(theme))
{
    m_popupCountConnection =
        QObject::connect(&m_presentation,
                         &Services::NotificationPresentationModel::
                             NotificationPresentationController::popupCountChanged,
                         &m_engine, [this] { updateVisibility(); });
    m_centerOpenConnection =
        QObject::connect(&m_presentation,
                         &Services::NotificationPresentationModel::
                             NotificationPresentationController::centerOpenChanged,
                         &m_engine, [this] { updateVisibility(); });
    m_operationBusyConnection =
        QObject::connect(&m_presentation,
                         &Services::NotificationPresentationModel::
                             NotificationPresentationController::operationBusyChanged,
                         &m_engine, [this] { updateVisibility(); });
    m_operationErrorConnection =
        QObject::connect(
            &m_presentation,
            &Services::NotificationPresentationModel::
                NotificationPresentationController::operationErrorTextChanged,
            &m_engine, [this] { updateVisibility(); });
}

NotificationWindowController::~NotificationWindowController()
{
    // AGENT-GUARD: both senders outlive this non-QObject controller during
    // shell teardown. Disconnect before invalidating the lambdas' `this`.
    QObject::disconnect(m_popupCountConnection);
    QObject::disconnect(m_centerOpenConnection);
    QObject::disconnect(m_operationBusyConnection);
    QObject::disconnect(m_operationErrorConnection);
    reset();
}

bool NotificationWindowController::reconcile(QScreen *screen, QString *error)
{
    if (screen == m_screen && m_popupWindow && m_centerWindow) {
        if (!resizeWindows(*screen, m_presentation.popupCount(), error)) {
            return false;
        }
        updateVisibility();
        return true;
    }
    reset();
    if (screen == nullptr) {
        setError(error, {});
        return true;
    }
    if (!createWindows(*screen, error)) {
        reset();
        return false;
    }
    m_screen = screen;
    updateVisibility();
    return true;
}

void NotificationWindowController::reset() noexcept
{
    if (m_popupWindow) {
        m_popupWindow->hide();
    }
    if (m_centerWindow) {
        m_centerWindow->hide();
    }
    m_popupWindow.reset();
    m_centerWindow.reset();
    m_screen.clear();
}

bool NotificationWindowController::ensureComponents(QString *error)
{
    if (!m_popupComponent) {
        m_popupComponent = std::make_unique<QQmlComponent>(&m_engine);
        m_popupComponent->loadFromModule(QStringLiteral("QindaQt.Shell.Runtime"),
                                         QStringLiteral("NotificationPopupStack"));
    }
    if (!m_centerComponent) {
        m_centerComponent = std::make_unique<QQmlComponent>(&m_engine);
        m_centerComponent->loadFromModule(QStringLiteral("QindaQt.Shell.Runtime"),
                                          QStringLiteral("NotificationCenter"));
    }
    if (!m_popupComponent->isReady() || !m_centerComponent->isReady()) {
        setError(error,
                 QStringLiteral("cannot load notification QML: %1\n%2")
                     .arg(componentErrors(*m_popupComponent),
                          componentErrors(*m_centerComponent)));
        return false;
    }
    return true;
}

std::unique_ptr<QQuickWindow> NotificationWindowController::createWindow(
    QQmlComponent &component, const QString &role, QString *error)
{
    QVariantMap properties = {
        {QStringLiteral("theme"), m_theme},
        {QStringLiteral("presentation"),
         QVariant::fromValue(static_cast<QObject *>(&m_presentation))},
    };
    if (role == QLatin1String("center")) {
        properties.insert(QStringLiteral("quietingSettings"),
                          QVariant::fromValue(static_cast<QObject *>(&m_quietingSettings)));
        properties.insert(QStringLiteral("settingsLauncher"),
                          QVariant::fromValue(static_cast<QObject *>(&m_settingsLauncher)));
    }
    QObject *created = component.createWithInitialProperties(properties);
    auto *window = qobject_cast<QQuickWindow *>(created);
    if (window == nullptr) {
        delete created;
        setError(error,
                 QStringLiteral("notification %1 QML did not create a window: %2")
                     .arg(role, componentErrors(component)));
        return {};
    }
    if (window->isVisible()) {
        window->hide();
    }
    window->setObjectName(QStringLiteral("qindaqt-notification-%1").arg(role));
    return std::unique_ptr<QQuickWindow>(window);
}

bool NotificationWindowController::createWindows(QScreen &screen, QString *error)
{
    if (!ensureComponents(error)) {
        return false;
    }
    auto popup = createWindow(*m_popupComponent, QStringLiteral("popups"), error);
    auto center = createWindow(*m_centerComponent, QStringLiteral("center"), error);
    if (!popup || !center) {
        return false;
    }
    const auto layout = ShellSurface::NotificationSurfaceLayoutPlanner::plan(
        screen.geometry().size(), PopupMargins, CenterMargins,
        m_presentation.popupCount());
    if (!layout.ok()) {
        setError(error, layout.error);
        return false;
    }
    if (!ShellSurface::LayerShellNotificationSurface::configure(
            *popup, screen, ShellSurface::NotificationSurfaceRole::PopupStack,
            layout.layout->popupSize, PopupMargins, error) ||
        !ShellSurface::LayerShellNotificationSurface::configure(
            *center, screen, ShellSurface::NotificationSurfaceRole::Center,
            layout.layout->centerSize, CenterMargins, error)) {
        return false;
    }
    m_popupWindow = std::move(popup);
    m_centerWindow = std::move(center);
    return true;
}

bool NotificationWindowController::resizeWindows(QScreen &screen, int popupCount,
                                                  QString *error)
{
    const auto layout = ShellSurface::NotificationSurfaceLayoutPlanner::plan(
        screen.geometry().size(), PopupMargins, CenterMargins, popupCount);
    if (!layout.ok()) {
        setError(error, layout.error);
        return false;
    }
    if (!ShellSurface::LayerShellNotificationSurface::resize(
            *m_popupWindow, layout.layout->popupSize, error) ||
        !ShellSurface::LayerShellNotificationSurface::resize(
            *m_centerWindow, layout.layout->centerSize, error)) {
        return false;
    }
    setError(error, {});
    return true;
}

void NotificationWindowController::updateVisibility()
{
    if (!m_popupWindow || !m_centerWindow || !m_screen) {
        return;
    }
    const int popupCount = m_presentation.popupCount();
    QString ignored;
    if (!resizeWindows(*m_screen, popupCount, &ignored)) {
        m_popupWindow->hide();
        m_centerWindow->hide();
        return;
    }
    // AGENT-GUARD: operation feedback lives in the popup header. Keep the
    // header-only surface mapped after a rejection or while an operation is
    // pending, even when the originating notification is no longer in the
    // popup model. The center presents the same status when it is open.
    const bool hasPopupSurfaceContent =
        popupCount > 0 || m_presentation.operationBusy() ||
        !m_presentation.operationErrorText().isEmpty();
    if (hasPopupSurfaceContent && !m_presentation.centerOpen()) {
        m_popupWindow->show();
    } else {
        m_popupWindow->hide();
    }
    if (m_presentation.centerOpen()) {
        m_centerWindow->show();
    } else {
        m_centerWindow->hide();
    }
}

} // namespace QindaQt::Shell
