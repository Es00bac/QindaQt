// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_popup_fixture.h"
#include "compositorprobeclient.h"
#include "notificationliveruntime.h"
#include "notificationlivesurfaces.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <qindaqt/services/notification_presentation/presentation_access_token.h>
#include <qindaqt/session_supervisor/tokenized_process_launcher.h>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QRegularExpression>
#include <unistd.h>

namespace QindaQt::Test {
namespace {
const QString SessionName = QStringLiteral("org.qindaqt.Session1");
const QString NotificationName = QStringLiteral("org.freedesktop.Notifications");
void stopChild(QProcess &child)
{
    if (child.state() == QProcess::NotRunning) return;
    child.terminate();
    if (!child.waitForFinished(2000)) {
        child.kill();
        child.waitForFinished(2000);
    }
}
bool privateModelsCleared(const QJsonObject &snapshot)
{
    const auto presentation = presentationEvidence(snapshot);
    return !presentation.value(QStringLiteral("privatePresentationAllowed")).toBool()
        && !presentation.value(QStringLiteral("centerOpen")).toBool()
        && presentation.value(QStringLiteral("activeCount")).toInt(-1) == 0
        && presentation.value(QStringLiteral("popupCount")).toInt(-1) == 0
        && presentation.value(QStringLiteral("historyCount")).toInt(-1) == 0
        && !windowEvidence(snapshot, QLatin1StringView("popup")).value(QStringLiteral("visible")).toBool()
        && !windowEvidence(snapshot, QLatin1StringView("center")).value(QStringLiteral("visible")).toBool();
}
}
NativePopupFixture::NativePopupFixture(QString row)
    : session(QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"),
          QStringLiteral("native-popup-session-") + row)), m_row(std::move(row)),
      m_connectionName(QStringLiteral("native-popup-session-") + m_row)
{}
NativePopupFixture::~NativePopupFixture()
{
    stopChild(shell);
    stopChild(host);
    stopChild(settings);
    monitor.reset();
    transport.reset();
    attachment.reset();
    session.unregisterService(SessionName);
    QDBusConnection::disconnectFromBus(m_connectionName);
}
bool NativePopupFixture::start(QString *error)
{
    auto bus = QDBusConnection::sessionBus();
    auto *const daemon = bus.interface();
    if (!daemon || !session.isConnected() || !session.registerService(SessionName)) {
        *error = QStringLiteral("test-owned Session1 selection failed"); return false;
    }
    daemon->setTimeout(250);
    selectedOwner = session.baseService();
    const auto owner = daemon->serviceOwner(QString(CompositorNames::service));
    const auto pid = daemon->servicePid(QString(CompositorNames::service));
    const auto uid = daemon->serviceUid(QString(CompositorNames::service));
    compositorPid = qEnvironmentVariable("QINDAQT_NATIVE_POPUP_COMPOSITOR_PID").toLongLong();
    if (!owner.isValid() || !pid.isValid() || !uid.isValid() || compositorPid <= 1
            || quint64(pid.value()) != quint64(compositorPid)
            || uid.value() != ::getuid()) {
        *error = QStringLiteral("actual compositor daemon UID/PID identity mismatch"); return false;
    }
    compositorOwner = owner.value();
    attachment = std::make_unique<Platform::Compositor::CompositorAttachment>(bus,
        qEnvironmentVariable("XDG_RUNTIME_DIR"), [this](const QString &candidate) {
            const auto current = QDBusConnection::sessionBus().interface()->serviceOwner(SessionName);
            return current.isValid() && candidate == selectedOwner && current.value() == candidate;
        });
    if (!attachment->attach(selectedOwner, qEnvironmentVariable("WAYLAND_DISPLAY"),
            Platform::Compositor::PeerExpectation{compositorOwner, quint64(compositorPid)})) {
        *error = QStringLiteral("actual ordinary socket/PIDFD admission failed"); return false;
    }
    transport = std::make_unique<Services::SessionLockState::QtNativeLockTransport>(bus);
    // Both the monitor and diagnostic subscription consume real targeted native
    // receipts. This listener cannot publish state to the shell or the fork.
    if (!bus.connect(compositorOwner, QString(CompositorNames::nativeLockPath),
            QString(CompositorNames::nativeLockInterface), QStringLiteral("stateReceipt"),
            this, SLOT(receipt(QString,bool,bool,QDBusMessage)))) {
        *error = QStringLiteral("native receipt diagnostic subscription failed"); return false;
    }
    monitor = std::make_unique<Services::SessionLockState::NativeLockStateMonitor>(*transport,
        [this](const QString &candidate, quint64 candidatePid) {
            const auto identity = attachment->identity();
            return identity && attachment->sameBus(QDBusConnection::sessionBus())
                && identity->compositorOwner == candidate && identity->compositorPid == candidatePid;
        });
    connect(monitor.get(), &Services::SessionLockState::NativeLockStateMonitor::stateChanged,
            this, [this](auto state) { states.append(state); });
    connect(attachment.get(), &Platform::Compositor::CompositorAttachment::revoked,
            monitor.get(), &Services::SessionLockState::NativeLockStateMonitor::refresh);
    if (!monitor->start(error) || !awaitNotificationLiveCondition([this] {
            return monitor->contentMayBeShown() && !receiptNonces.isEmpty();
        })) return false;
    const auto logRoot = qEnvironmentVariable("QINDAQT_NATIVE_POPUP_LOG_ROOT");
    for (auto *child : {&settings, &host, &shell}) {
        child->setProcessChannelMode(QProcess::MergedChannels);
    }
    settings.setStandardOutputFile(logRoot + QLatin1Char('/') + m_row + QStringLiteral("-settings.log"));
    settings.start(qEnvironmentVariable("QINDAQT_NATIVE_POPUP_SETTINGS"), QStringList{});
    if (!settings.waitForStarted(5000) || !awaitNotificationLiveService(QStringLiteral("org.qindaqt.Settings1"))) {
        *error = QStringLiteral("production Settings1 did not start"); return false;
    }
    const auto token = Services::NotificationPresentation::PresentationAccessToken::generate();
    host.setStandardOutputFile(logRoot + QLatin1Char('/') + m_row + QStringLiteral("-host.log"));
    shell.setStandardOutputFile(logRoot + QLatin1Char('/') + m_row + QStringLiteral("-shell.log"));
    if (!SessionSupervisor::TokenizedProcessLauncher::start(host,
            qEnvironmentVariable("QINDAQT_NATIVE_POPUP_HOST"), {}, token, error)
        || !awaitNotificationLiveService(NotificationName)
        || !SessionSupervisor::TokenizedProcessLauncher::start(shell,
            qEnvironmentVariable("QINDAQT_NATIVE_POPUP_SHELL"),
            {QStringLiteral("--compositor-pid"), QString::number(compositorPid)}, token, error)) return false;
    shellEvidence = std::make_unique<NotificationLiveEvidenceClient>();
    const bool admitted = shellEvidence->authenticate(shell.processId(), error)
        && shellEvidence->awaitSnapshot([](const auto &snapshot) {
            return presentationEvidence(snapshot).value(QStringLiteral("privatePresentationAllowed")).toBool();
        }, error).has_value();
    if (!admitted) return false;
    // Establish an actual host snapshot baseline before the measured Notify.
    // Startup notification-client readiness has no exported test-only switch.
    quint32 baseline = 0;
    if (!submitCritical(error, &baseline) || !shellEvidence->awaitSnapshot([](const auto &value) {
            return presentationEvidence(value).value(QStringLiteral("activeCount")).toInt(-1) == 1;
        }, error)) return false;
    QDBusInterface notifications(NotificationName, QStringLiteral("/org/freedesktop/Notifications"), NotificationName);
    const auto closed = notifications.call(QStringLiteral("CloseNotification"), baseline);
    return closed.type() != QDBusMessage::ErrorMessage
        && shellEvidence->awaitSnapshot([](const auto &value) {
            return presentationEvidence(value).value(QStringLiteral("activeCount")).toInt(-1) == 0
                && presentationEvidence(value).value(QStringLiteral("popupCount")).toInt(-1) == 0;
        }, error).has_value();
}
void NativePopupFixture::receipt(const QString &nonce, bool, bool, const QDBusMessage &message)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{32}$"));
    if (message.service() != compositorOwner || message.signature() != QLatin1String("sbb")
        || !pattern.match(nonce).hasMatch()) { malformedReceipt = true; return; }
    receiptNonces.insert(nonce);
}
bool NativePopupFixture::submitCritical(QString *error, quint32 *id)
{
    QDBusInterface notifications(NotificationName, QStringLiteral("/org/freedesktop/Notifications"), NotificationName);
    const QVariantMap hints{{QStringLiteral("urgency"), QVariant::fromValue(uchar(2))},
                           {QStringLiteral("transient"), true}};
    const QDBusReply<quint32> reply = notifications.call(QStringLiteral("Notify"),
        QStringLiteral("Native popup qualification"), quint32(0), QString{},
        QStringLiteral("Private production notification"), QStringLiteral("Native privacy boundary"),
        QStringList{}, hints, 0);
    if (!reply.isValid() || reply.value() == 0) {
        *error = QStringLiteral("actual resident Notify failed: %1").arg(reply.error().message()); return false;
    }
    if (id) *id = reply.value();
    return true;
}
bool NativePopupFixture::mapPopup(QJsonObject *evidence, QString *error)
{
    if (!submitCritical(error)) return false;
    const auto snapshot = shellEvidence->awaitSnapshot([](const auto &value) {
        return presentationEvidence(value).value(QStringLiteral("popupCount")).toInt(-1) == 1
            && windowEvidence(value, QLatin1StringView("popup")).value(QStringLiteral("visible")).toBool();
    }, error);
    if (!snapshot) return false;
    CompositorProbeClient compositor;
    NotificationLiveExpectations expected;
    expected.shellProcessId = shell.processId();
    expected.logicalWidth = 1920; expected.logicalHeight = 1080; expected.scale = 1.0;
    if (!validateNotificationLiveSurface(QLatin1StringView("notification-popup"), *snapshot, expected, compositor, error)) return false;
    const auto surfaces = compositor.developmentShellSurfaces(error);
    if (!surfaces) return false;
    evidence->insert(QStringLiteral("mappedSurfaces"), *surfaces);
    evidence->insert(QStringLiteral("shellPid"), QString::number(shell.processId()));
    evidence->insert(QStringLiteral("compositorPid"), QString::number(compositorPid));
    evidence->insert(QStringLiteral("selectedSessionOwner"), selectedOwner);
    evidence->insert(QStringLiteral("compositorOwner"), compositorOwner);
    evidence->insert(QStringLiteral("receiptNonceCount"), receiptNonces.size());
    return true;
}
bool NativePopupFixture::retired(QJsonObject *evidence, QString *error)
{
    const auto snapshot = shellEvidence->awaitSnapshot(privateModelsCleared, error);
    if (!snapshot) return false;
    CompositorProbeClient compositor;
    QJsonArray last;
    if (!awaitNotificationLiveCondition([&] {
        const auto surfaces = compositor.developmentShellSurfaces(error);
        if (!surfaces) return false;
        last = *surfaces;
        for (const auto &value : last) {
            const auto surface = value.toObject();
            if (surface.value(QStringLiteral("processId")).toString().toLongLong() == shell.processId()
                && (surface.value(QStringLiteral("scope")) == QStringLiteral("notification-popup")
                    || surface.value(QStringLiteral("scope")) == QStringLiteral("notification-center"))
                && surface.value(QStringLiteral("mapped")).toBool()) return false;
        }
        return true;
    })) { *error = QStringLiteral("private notification surface remained compositor-mapped"); return false; }
    evidence->insert(QStringLiteral("retiredSurfaces"), last);
    evidence->insert(QStringLiteral("privacySnapshot"), *snapshot);
    return shell.state() != QProcess::NotRunning && host.state() != QProcess::NotRunning;
}
}
