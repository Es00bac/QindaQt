// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/resident_portal_service.h"
#include "portal_frontend_test_support.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>

using namespace QindaQt::Services::Portal;
using namespace QindaQt::Tests::Portal;

namespace {

class FrontendChangeReceiver final : public QObject {
    Q_OBJECT
public Q_SLOTS:
    void receive(const QString &namespaceName, const QString &key,
                 const QDBusVariant &value)
    {
        if (namespaceName == QString::fromLatin1(kAppearanceNamespace)) {
            values.insert(key, value.variant());
            types.insert(key, QString::fromLatin1(value.variant().metaType().name()));
        }
    }
public:
    QVariantMap values;
    QMap<QString, QString> types;
};

class FakeFileChooser final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
public:
    [[nodiscard]] quint32 version() const noexcept { return 4; }
    [[nodiscard]] int calls() const noexcept { return m_calls; }

public Q_SLOTS:
    Q_SCRIPTABLE quint32 OpenFile(const QDBusObjectPath &, const QString &,
                                 const QString &, const QString &title,
                                 const QVariantMap &, QVariantMap &results)
    {
        ++m_calls;
        m_lastTitle = title;
        results.clear();
        return 1;
    }

public:
    [[nodiscard]] const QString &lastTitle() const noexcept { return m_lastTitle; }

private:
    int m_calls = 0;
    QString m_lastTitle;
};

class FakeGlobalShortcuts final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.GlobalShortcuts")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
public:
    explicit FakeGlobalShortcuts(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 1; }
    [[nodiscard]] int calls() const noexcept { return m_calls; }

public Q_SLOTS:
    Q_SCRIPTABLE quint32 CreateSession(const QDBusObjectPath &, const QDBusObjectPath &,
                                       const QString &, const QVariantMap &,
                                       QVariantMap &results)
    {
        ++m_calls;
        results.clear();
        return 0;
    }

private:
    int m_calls = 0;
};

bool kdePortalAdvertisesGlobalShortcuts(QString *error)
{
    QFile metadata(QString::fromUtf8(QINDAQT_FALLBACK_PORTAL));
    if (!metadata.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("cannot read installed KDE portal metadata %1")
                     .arg(metadata.fileName());
        return false;
    }
    if (!metadata.readAll().contains("org.freedesktop.impl.portal.GlobalShortcuts")) {
        *error = QStringLiteral(
            "installed KDE portal backend %1 no longer advertises GlobalShortcuts; "
            "the qindaqt-portals.conf fallback entry needs re-review")
                     .arg(metadata.fileName());
        return false;
    }
    return true;
}

FakeGlobalShortcuts *registerInjectedServices(QDBusConnection &bus,
                                              FakeFileChooser &fallback,
                                              QString *error)
{
    // AGENT-GUARD: Owning these names before the frontend starts prevents the
    // private bus from activating installed host helpers during this proof.
    auto *shortcuts = new FakeGlobalShortcuts(&fallback);
    if (!bus.registerObject(QString::fromLatin1(kPortalObjectPath), &fallback,
                            QDBusConnection::ExportScriptableSlots
                                | QDBusConnection::ExportScriptableProperties
                                | QDBusConnection::ExportAdaptors)
        || !bus.registerService(QString::fromLatin1(FallbackService))
        || !bus.registerService(QString::fromLatin1(DocumentsService))
        || !bus.registerService(QString::fromLatin1(PermissionStoreService))) {
        *error = QStringLiteral("cannot register injected private-bus portal services");
        return nullptr;
    }
    return shortcuts;
}

bool runSelection(QString *error)
{
    if (!kdePortalAdvertisesGlobalShortcuts(error)) {
        return false;
    }
    auto runtime = stageRuntime(error);
    if (!runtime.has_value()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakeFileChooser fallback;
    FakeGlobalShortcuts *shortcuts = registerInjectedServices(bus, fallback, error);
    if (shortcuts == nullptr) {
        return false;
    }
    ChildProcesses children;
    QProcess *frontend = nullptr;
    if (!startCore(*runtime, children, &frontend, error)
        || !verifyFrontendAppearance(error)) {
        return false;
    }

    FrontendChangeReceiver receiver;
    if (!bus.connect(QString::fromLatin1(FrontendService),
                     QString::fromLatin1(kPortalObjectPath),
                     QString::fromLatin1(FrontendInterface),
                     QStringLiteral("SettingChanged"), &receiver,
                     SLOT(receive(QString,QString,QDBusVariant)))
        || !commitColorScheme(QStringLiteral("light"), error)
        || !waitUntil([&] {
               return unwrapVariant(receiver.values.value(
                                          QString::fromLatin1(kColorSchemeKey))).toUInt() == 2;
           }, 5'000)) {
        *error = error->isEmpty()
            ? QStringLiteral("frontend did not propagate the confirmed live change")
            : *error;
        return false;
    }

    QDBusMessage chooser = QDBusMessage::createMethodCall(
        QString::fromLatin1(FrontendService), QString::fromLatin1(kPortalObjectPath),
        QStringLiteral("org.freedesktop.portal.FileChooser"),
        QStringLiteral("OpenFile"));
    chooser << QString{} << QStringLiteral("QindaQt fallback proof")
            << QVariantMap{{QStringLiteral("handle_token"),
                            QStringLiteral("qindaqt_fallback_proof")}};
    QDBusPendingCall pending = bus.asyncCall(chooser, 5'000);
    if (!waitUntil([&] { return fallback.calls() == 1; }, 5'000)
        || fallback.lastTitle() != QStringLiteral("QindaQt fallback proof")) {
        *error = QStringLiteral("FileChooser did not resolve to the declared KDE fallback");
        return false;
    }
    Q_UNUSED(pending)

    QDBusMessage globalShortcuts = QDBusMessage::createMethodCall(
        QString::fromLatin1(FrontendService), QString::fromLatin1(kPortalObjectPath),
        QStringLiteral("org.freedesktop.portal.GlobalShortcuts"),
        QStringLiteral("CreateSession"));
    globalShortcuts << QVariantMap{
        {QStringLiteral("handle_token"),
         QStringLiteral("qindaqt_globalshortcuts_proof")},
        {QStringLiteral("session_handle_token"),
         QStringLiteral("qindaqt_globalshortcuts_proof_session")}};
    QDBusPendingCall shortcutsPending = bus.asyncCall(globalShortcuts, 5'000);
    if (!waitUntil([&] { return shortcuts->calls() == 1; }, 5'000)) {
        *error = QStringLiteral(
            "GlobalShortcuts did not resolve to the declared KDE fallback");
        return false;
    }
    Q_UNUSED(shortcutsPending)

    const QDBusMessage background = frontendPortalCall(
        QStringLiteral("org.freedesktop.portal.Background"),
        QStringLiteral("GetAppState"));
    if (background.type() != QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("Background unexpectedly escaped default=none");
        return false;
    }

    ChildProcesses::stop(*frontend);
    runtime->environment.insert(QStringLiteral("XDG_CURRENT_DESKTOP"),
                                QStringLiteral("other-desktop"));
    frontend = children.start(QString::fromUtf8(QINDAQT_XDG_DESKTOP_PORTAL),
                              {QStringLiteral("--verbose")},
                              runtime->environment, error);
    if (frontend == nullptr || !waitForService(QString::fromLatin1(FrontendService))) {
        *error = QStringLiteral("other-desktop frontend did not start");
        return false;
    }
    const QDBusMessage negative = frontendPortalCall(
        QString::fromLatin1(FrontendInterface),
        QStringLiteral("Read"),
        {QString::fromLatin1(kAppearanceNamespace),
         QString::fromLatin1(kColorSchemeKey)});
    if (negative.type() != QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("other desktop selected the QindaQt Settings backend");
        return false;
    }
    return true;
}

bool runToolkit(QString *error)
{
    auto runtime = stageRuntime(error);
    if (!runtime.has_value()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakeFileChooser fallback;
    if (registerInjectedServices(bus, fallback, error) == nullptr) {
        return false;
    }
    ChildProcesses children;
    QProcess *frontend = nullptr;
    if (!startCore(*runtime, children, &frontend, error)
        || !verifyFrontendAppearance(error)) {
        return false;
    }
    QProcessEnvironment probeEnvironment = runtime->environment;
    probeEnvironment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    probeEnvironment.insert(QStringLiteral("QT_QPA_PLATFORMTHEME"),
                            QStringLiteral("xdgdesktopportal"));
    QProcess *probe = children.start(
        configuredPath("QINDAQT_TEST_TOOLKIT_PROBE", QINDAQT_TOOLKIT_PROBE),
        {}, probeEnvironment, error);
    QByteArray probeOutput;
    FrontendChangeReceiver receiver;
    if (!bus.connect(QString::fromLatin1(FrontendService),
                     QString::fromLatin1(kPortalObjectPath),
                     QString::fromLatin1(FrontendInterface),
                     QStringLiteral("SettingChanged"), &receiver,
                     SLOT(receive(QString,QString,QDBusVariant)))) {
        *error = QStringLiteral("cannot observe toolkit frontend changes");
        return false;
    }
    if (probe == nullptr
        || !waitUntil([&] {
               probeOutput.append(probe->readAllStandardOutput());
               return probeOutput.contains("READY dark palette");
           }, 11'000)
        || !waitUntil([started = QElapsedTimer{}]() mutable {
               if (!started.isValid()) {
                   started.start();
               }
               return started.elapsed() >= 250;
           }, 500)
        || !commitColorScheme(QStringLiteral("light"), error)
        || !waitUntil([&] {
               return unwrapVariant(receiver.values.value(
                                          QString::fromLatin1(kColorSchemeKey))).toUInt() == 2
                      && probe->state() == QProcess::NotRunning;
           }, 11'000)) {
        *error = error->isEmpty()
            ? QStringLiteral("Qt portal-theme probe did not reach its live-change result: %1 %2")
                  .arg(QString::fromUtf8(probeOutput),
                       QString::fromUtf8(probe == nullptr
                                             ? QByteArray{}
                                             : probe->readAllStandardError()))
                  + QStringLiteral(" frontend-type=%1")
                        .arg(receiver.types.value(
                            QString::fromLatin1(kColorSchemeKey)))
            : *error;
        return false;
    }
    probeOutput.append(probe->readAllStandardOutput());
    if (probe->exitStatus() != QProcess::NormalExit || probe->exitCode() != 0
        || !probeOutput.contains("CHANGED light palette")) {
        *error = QStringLiteral("Qt portal-theme probe failed: %1")
                     .arg(QString::fromUtf8(probe->readAllStandardError()));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (!QDBusConnection::sessionBus().isConnected()
        || application.arguments().size() != 2) {
        qCritical("portal frontend test requires one private-bus mode argument");
        return 2;
    }
    QString error;
    const QString mode = application.arguments().at(1);
    const bool passed = mode == QStringLiteral("selection")
        ? runSelection(&error)
        : mode == QStringLiteral("toolkit") ? runToolkit(&error) : false;
    if (!passed) {
        qCritical().noquote() << (error.isEmpty() ? QStringLiteral("unknown test mode") : error);
        return 1;
    }
    return 0;
}

#include "tst_portal_frontend_integration.moc"
