// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelldevelopmentevidence.h"

#include "notificationwindowcontroller.h"

#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"
#include "qindaqt/services/notification_presentation_policy/notification_privacy_policy.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QTimer>

#include <utility>

namespace QindaQt::Shell {
namespace {

constexpr auto CompositorService = "org.qindaqt.Compositor";
constexpr auto CompositorObject = "/org/qindaqt/Compositor";
constexpr auto CompositorInterface = "org.qindaqt.Compositor1";

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

QString quietingStateName(
    const Services::SettingsClient::DoNotDisturbController &controller)
{
    if (controller.loading()) {
        return QStringLiteral("loading");
    }
    if (controller.ready()) {
        return QStringLiteral("ready");
    }
    if (controller.saving()) {
        return QStringLiteral("saving");
    }
    if (controller.conflict()) {
        return QStringLiteral("conflict");
    }
    if (controller.unavailable()) {
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unknown");
}

bool surfaceVisible(const QJsonObject &windows, QLatin1StringView name)
{
    return windows.value(name).toObject().value(QStringLiteral("visible")).toBool();
}

} // namespace

ShellDevelopmentEvidence::ShellDevelopmentEvidence(
    Services::NotificationPresentationModel::NotificationPresentationController &
        presentation,
    Services::SettingsClient::DoNotDisturbController &quieting,
    Services::NotificationPresentationPolicy::NotificationPrivacyPolicy &privacy,
    NotificationWindowController &windows, QObject *parent)
    : QObject(parent)
    , m_presentation(presentation)
    , m_quieting(quieting)
    , m_privacy(privacy)
    , m_windows(windows)
    , m_bus(QDBusConnection::sessionBus())
{
    connect(&m_presentation,
            &Services::NotificationPresentationModel::
                NotificationPresentationController::centerOpenChanged,
            this, &ShellDevelopmentEvidence::observeCenterChange);
    connect(&m_presentation,
            &Services::NotificationPresentationModel::
                NotificationPresentationController::operationBusyChanged,
            this, &ShellDevelopmentEvidence::observeBusyChange);
    connect(&m_presentation,
            &Services::NotificationPresentationModel::
                NotificationPresentationController::operationErrorTextChanged,
            this, &ShellDevelopmentEvidence::observeErrorChange);
    connect(&m_privacy,
            &Services::NotificationPresentationPolicy::NotificationPrivacyPolicy::
                privatePresentationAllowedChanged,
            this, &ShellDevelopmentEvidence::observePrivacyChange);
    connect(&m_quieting,
            &Services::SettingsClient::DoNotDisturbController::stateChanged,
            this, &ShellDevelopmentEvidence::observeQuietingChange);
}

ShellDevelopmentEvidence::~ShellDevelopmentEvidence()
{
    if (m_registeredObject) {
        m_bus.unregisterObject(QString::fromLatin1(ObjectPath));
    }
    if (m_registeredService) {
        m_bus.unregisterService(QString::fromLatin1(ServiceName));
    }
}

bool ShellDevelopmentEvidence::start(qint64 compositorProcessId,
                                     qint64 predecessorShellProcessId,
                                     QString *error)
{
    if (qEnvironmentVariable("QINDAQT_DEVELOPMENT_CONTROL") != QLatin1String("1")) {
        setError(error, QStringLiteral("shell development evidence is disabled"));
        return false;
    }
    if (!m_bus.isConnected() || compositorProcessId <= 1) {
        setError(error, QStringLiteral("shell development evidence has no private runtime"));
        return false;
    }
    auto *const busInterface = m_bus.interface();
    if (busInterface == nullptr) {
        setError(error, QStringLiteral("shell development evidence has no bus daemon"));
        return false;
    }
    const QDBusReply<uint> compositorPid =
        busInterface->servicePid(QString::fromLatin1(CompositorService));
    if (!compositorPid.isValid()
        || static_cast<qint64>(compositorPid.value()) != compositorProcessId) {
        setError(error, QStringLiteral("shell development evidence rejected compositor PID"));
        return false;
    }
    QDBusInterface compositor(QString::fromLatin1(CompositorService),
                              QString::fromLatin1(CompositorObject),
                              QString::fromLatin1(CompositorInterface), m_bus);
    const QDBusReply<QByteArray> capabilities =
        compositor.call(QStringLiteral("Capabilities"));
    const QJsonObject capabilityObject =
        QJsonDocument::fromJson(capabilities.value()).object();
    if (!capabilities.isValid()
        || capabilityObject.value(QStringLiteral("controlMode"))
               != QStringLiteral("development-test")
        || !capabilityObject.value(QStringLiteral("mutationsEnabled")).toBool()) {
        setError(error,
                 QStringLiteral("shell development evidence rejected production control"));
        return false;
    }

    if (!registerServiceAfterPredecessor(predecessorShellProcessId, error)) {
        return false;
    }
    m_registeredObject = m_registeredService
        && m_bus.registerObject(QString::fromLatin1(ObjectPath), this,
                                QDBusConnection::ExportScriptableSlots);
    if (!m_registeredObject) {
        if (m_registeredService) {
            m_bus.unregisterService(QString::fromLatin1(ServiceName));
            m_registeredService = false;
        }
        setError(error, QStringLiteral("shell development evidence registration failed"));
        return false;
    }
    setError(error, {});
    return true;
}

bool ShellDevelopmentEvidence::registerServiceAfterPredecessor(
    qint64 predecessorShellProcessId, QString *error)
{
    auto *const interface = m_bus.interface();
    if (interface == nullptr) {
        setError(error, QStringLiteral("shell development evidence has no bus daemon"));
        return false;
    }
    const QString service = QString::fromLatin1(ServiceName);
    const QDBusReply<bool> registered = interface->isServiceRegistered(service);
    if (!registered.isValid()) {
        setError(error, QStringLiteral("could not inspect shell evidence owner"));
        return false;
    }
    if (registered.value()) {
        const QDBusReply<QString> owner = interface->serviceOwner(service);
        const QDBusReply<uint> processId = interface->servicePid(service);
        const QDBusReply<bool> stillRegistered =
            interface->isServiceRegistered(service);
        if (!stillRegistered.isValid()) {
            setError(error,
                     QStringLiteral("could not recheck shell evidence owner"));
            return false;
        }
        if (!stillRegistered.value()) {
            // The authenticated predecessor released between the initial name
            // check and credential queries. DontQueue below remains the only
            // acquisition attempt and will reject any racing new owner.
        } else if (!owner.isValid() || owner.value().isEmpty()
                   || !processId.isValid() || predecessorShellProcessId <= 1
                   || static_cast<qint64>(processId.value())
                       != predecessorShellProcessId) {
            setError(error,
                     QStringLiteral("shell evidence name has an unexpected owner"));
            return false;
        } else {
            bool released = false;
            QEventLoop loop;
            const QMetaObject::Connection ownerChange = connect(
                interface, &QDBusConnectionInterface::serviceOwnerChanged, &loop,
                [&](const QString &name, const QString &oldOwner,
                    const QString &newOwner) {
                    if (name == service && oldOwner == owner.value()
                        && newOwner.isEmpty()) {
                        released = true;
                        loop.quit();
                    }
                });
            QTimer deadline;
            deadline.setSingleShot(true);
            deadline.setInterval(1'000);
            connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
            const QDBusReply<QString> afterConnect =
                interface->serviceOwner(service);
            if (afterConnect.isValid()
                && afterConnect.value() == owner.value()) {
                deadline.start();
                loop.exec();
            } else if (!afterConnect.isValid()
                       || afterConnect.value().isEmpty()) {
                released = true;
            }
            disconnect(ownerChange);
            if (!released) {
                setError(error,
                         QStringLiteral("prior shell evidence owner did not release"));
                return false;
            }
        }
    }

    // AGENT-GUARD: Never queue behind or replace a D-Bus owner. At most one
    // predecessor-PID-authenticated release is awaited; any racing owner makes
    // this replacement fail closed.
    const QDBusReply<QDBusConnectionInterface::RegisterServiceReply> reply =
        interface->registerService(
            service, QDBusConnectionInterface::DontQueueService,
            QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid()
        || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        setError(error,
                 QStringLiteral("shell development evidence name is unavailable"));
        return false;
    }
    m_registeredService = true;
    return true;
}

QByteArray ShellDevelopmentEvidence::Snapshot() const
{
    return QJsonDocument(snapshotObject()).toJson(QJsonDocument::Compact);
}

QJsonObject ShellDevelopmentEvidence::snapshotObject() const
{
    const QJsonObject windows = m_windows.evidence();
    return {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("shellPid"),
         QString::number(QCoreApplication::applicationPid())},
        {QStringLiteral("presentation"),
         QJsonObject{
             {QStringLiteral("privatePresentationAllowed"),
              m_presentation.privatePresentationAllowed()},
             {QStringLiteral("centerOpen"), m_presentation.centerOpen()},
             {QStringLiteral("activeCount"),
              m_presentation.activeModel()->rowCount()},
             {QStringLiteral("popupCount"), m_presentation.popupCount()},
             {QStringLiteral("historyCount"),
              m_presentation.historyModel()->rowCount()},
             {QStringLiteral("operationBusy"), m_presentation.operationBusy()},
             {QStringLiteral("operationErrorText"),
              m_presentation.operationErrorText()},
         }},
        {QStringLiteral("quieting"),
         QJsonObject{
             {QStringLiteral("enabled"), m_quieting.enabled()},
             {QStringLiteral("hasBaseline"), m_quieting.hasBaseline()},
             {QStringLiteral("state"), quietingStateName(m_quieting)},
             {QStringLiteral("canToggle"), m_quieting.canToggle()},
             {QStringLiteral("statusText"), m_quieting.statusText()},
             {QStringLiteral("errorText"), m_quieting.errorText()},
         }},
        {QStringLiteral("windows"), windows},
        {QStringLiteral("observations"),
         QJsonObject{
             {QStringLiteral("centerOpenedCount"),
              QString::number(m_centerOpenedCount)},
             {QStringLiteral("centerClosedCount"),
              QString::number(m_centerClosedCount)},
             {QStringLiteral("busyVisibleCount"),
              QString::number(m_busyVisibleCount)},
             {QStringLiteral("errorVisibleCount"),
              QString::number(m_errorVisibleCount)},
             {QStringLiteral("privacyDeniedClearCount"),
              QString::number(m_privacyDeniedClearCount)},
             {QStringLiteral("quietingStateChangeCount"),
              QString::number(m_quietingStateChangeCount)},
             {QStringLiteral("quietingSavingVisibleCount"),
              QString::number(m_quietingSavingVisibleCount)},
             {QStringLiteral("quietingErrorVisibleCount"),
              QString::number(m_quietingErrorVisibleCount)},
             {QStringLiteral("quietingUnavailableVisibleCount"),
              QString::number(m_quietingUnavailableVisibleCount)},
         }},
    };
}

void ShellDevelopmentEvidence::observeCenterChange()
{
    if (m_presentation.centerOpen()) {
        ++m_centerOpenedCount;
    } else {
        ++m_centerClosedCount;
    }
}

void ShellDevelopmentEvidence::observeBusyChange()
{
    if (!m_presentation.operationBusy()) {
        return;
    }
    QTimer::singleShot(0, this, [this] {
        const QJsonObject windows = m_windows.evidence();
        const QJsonObject popup = windows.value(QStringLiteral("popup")).toObject();
        const QJsonObject center = windows.value(QStringLiteral("center")).toObject();
        const bool popupStatus =
            popup.value(QStringLiteral("operationStatus"))
                .toObject()
                .value(QStringLiteral("visible"))
                .toBool();
        const bool centerStatus =
            center.value(QStringLiteral("operationStatus"))
                .toObject()
                .value(QStringLiteral("visible"))
                .toBool();
        if (m_presentation.operationBusy()
            && ((surfaceVisible(windows, QLatin1StringView("popup")) && popupStatus)
                || (surfaceVisible(windows, QLatin1StringView("center"))
                    && centerStatus))) {
            ++m_busyVisibleCount;
        }
    });
}

void ShellDevelopmentEvidence::observeErrorChange()
{
    if (m_presentation.operationErrorText().isEmpty()) {
        return;
    }
    QTimer::singleShot(0, this, [this] {
        const QJsonObject windows = m_windows.evidence();
        const QJsonObject popup = windows.value(QStringLiteral("popup")).toObject();
        const QJsonObject center = windows.value(QStringLiteral("center")).toObject();
        const bool popupStatus =
            popup.value(QStringLiteral("operationStatus"))
                .toObject()
                .value(QStringLiteral("visible"))
                .toBool();
        const bool centerStatus =
            center.value(QStringLiteral("operationStatus"))
                .toObject()
                .value(QStringLiteral("visible"))
                .toBool();
        if (!m_presentation.operationErrorText().isEmpty()
            && ((surfaceVisible(windows, QLatin1StringView("popup")) && popupStatus)
                || (surfaceVisible(windows, QLatin1StringView("center"))
                    && centerStatus))) {
            ++m_errorVisibleCount;
        }
    });
}

void ShellDevelopmentEvidence::observePrivacyChange(bool allowed)
{
    if (allowed) {
        return;
    }
    QTimer::singleShot(0, this, [this] {
        const QJsonObject windows = m_windows.evidence();
        if (!m_privacy.privatePresentationAllowed()
            && !m_presentation.centerOpen() && m_presentation.popupCount() == 0
            && m_presentation.activeModel()->rowCount() == 0
            && m_presentation.historyModel()->rowCount() == 0
            && !m_presentation.operationBusy()
            && m_presentation.operationErrorText().isEmpty()
            && !surfaceVisible(windows, QLatin1StringView("popup"))
            && !surfaceVisible(windows, QLatin1StringView("center"))) {
            ++m_privacyDeniedClearCount;
        }
    });
}

void ShellDevelopmentEvidence::observeQuietingChange()
{
    ++m_quietingStateChangeCount;
    const bool saving = m_quieting.saving();
    const bool rejected = m_quieting.ready() && !m_quieting.errorText().isEmpty();
    const bool unavailable = m_quieting.unavailable();
    // QML bindings observe the same state edge. Sample on the following event
    // turn so these counters prove the resulting status was actually visible,
    // independent of QObject connection ordering.
    QTimer::singleShot(0, this, [this, saving, rejected, unavailable] {
        const QJsonObject center =
            m_windows.evidence().value(QStringLiteral("center")).toObject();
        const QJsonObject status =
            center.value(QStringLiteral("quietingStatus")).toObject();
        const bool visible = center.value(QStringLiteral("visible")).toBool()
            && status.value(QStringLiteral("visible")).toBool();
        if (!visible) {
            return;
        }
        if (saving) {
            ++m_quietingSavingVisibleCount;
        }
        if (rejected) {
            ++m_quietingErrorVisibleCount;
        }
        if (unavailable) {
            ++m_quietingUnavailableVisibleCount;
        }
    });
}

} // namespace QindaQt::Shell
