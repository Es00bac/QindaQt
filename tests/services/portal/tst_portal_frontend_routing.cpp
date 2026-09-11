// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/resident_portal_service.h"
#include "portal_frontend_test_support.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusUnixFileDescriptor>
#include <QProcess>
#include <unistd.h>

using namespace QindaQt::Services::Portal;
using namespace QindaQt::Tests::Portal;

namespace {

// AGENT-GUARD: every fake returns impl version 1. The 1.20.4 frontend takes
// its simplest forwarding path for version-1 backends; claiming a newer
// version silently reroutes the proof through permission/dialog branches
// these fakes do not implement.
class FakePortalRoot final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
};

class FakeAccess final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Access")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
public:
    explicit FakeAccess(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 1; }
};

class FakeFileChooser final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
public:
    explicit FakeFileChooser(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 4; }
    [[nodiscard]] int calls() const noexcept { return m_calls; }
    [[nodiscard]] const QString &lastTitle() const noexcept { return m_lastTitle; }

public Q_SLOTS:
    Q_SCRIPTABLE quint32 OpenFile(const QDBusObjectPath &, const QString &,
                                  const QString &, const QString &title,
                                  const QVariantMap &, QVariantMap &results)
    {
        ++m_calls;
        m_lastTitle = title;
        results.clear();
        return 0;
    }

private:
    int m_calls = 0;
    QString m_lastTitle;
};

class FakeScreenshot final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Screenshot")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
public:
    explicit FakeScreenshot(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 1; }
    [[nodiscard]] int calls() const noexcept { return m_calls; }

public Q_SLOTS:
    Q_SCRIPTABLE quint32 Screenshot(const QDBusObjectPath &, const QString &,
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

class FakeScreenCast final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.ScreenCast")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
    Q_PROPERTY(quint32 AvailableSourceTypes READ availableSourceTypes CONSTANT SCRIPTABLE true)
    Q_PROPERTY(quint32 AvailableCursorModes READ availableCursorModes CONSTANT SCRIPTABLE true)
public:
    explicit FakeScreenCast(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 1; }
    [[nodiscard]] quint32 availableSourceTypes() const noexcept { return 1; }
    [[nodiscard]] quint32 availableCursorModes() const noexcept { return 1; }
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

class FakeRemoteDesktop final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.RemoteDesktop")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
    Q_PROPERTY(quint32 AvailableDeviceTypes READ availableDeviceTypes CONSTANT SCRIPTABLE true)
public:
    explicit FakeRemoteDesktop(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 1; }
    [[nodiscard]] quint32 availableDeviceTypes() const noexcept { return 7; }
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

class FakeInputCapture final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.InputCapture")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
    Q_PROPERTY(quint32 SupportedCapabilities READ supportedCapabilities CONSTANT SCRIPTABLE true)
public:
    explicit FakeInputCapture(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 1; }
    [[nodiscard]] quint32 supportedCapabilities() const noexcept { return 1; }
    [[nodiscard]] int calls() const noexcept { return m_calls; }

public Q_SLOTS:
    Q_SCRIPTABLE quint32 CreateSession(const QDBusObjectPath &, const QDBusObjectPath &,
                                       const QString &, const QString &,
                                       const QVariantMap &, QVariantMap &results)
    {
        ++m_calls;
        results.clear();
        return 0;
    }

private:
    int m_calls = 0;
};

class FakeSecret final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Secret")
    Q_PROPERTY(quint32 version READ version CONSTANT SCRIPTABLE true)
public:
    explicit FakeSecret(QObject *parent) : QDBusAbstractAdaptor(parent) {}
    [[nodiscard]] quint32 version() const noexcept { return 1; }
    [[nodiscard]] int calls() const noexcept { return m_calls; }

public Q_SLOTS:
    // AGENT-GUARD: the 1.20.4 frontend forwards RetrieveSecret to the backend
    // even for unsandboxed callers, so reaching this slot is the proof that
    // the Secret routing row resolves; never turn this fake into a secret
    // store.
    Q_SCRIPTABLE quint32 RetrieveSecret(const QDBusObjectPath &, const QString &,
                                        const QDBusUnixFileDescriptor &,
                                        const QVariantMap &, QVariantMap &results)
    {
        ++m_calls;
        results.clear();
        return 0;
    }

private:
    int m_calls = 0;
};

bool registerRoutingFakes(QDBusConnection &bus, FakePortalRoot &root,
                          FakeFileChooser **chooser, FakeScreenshot **screenshot,
                          FakeScreenCast **screencast, FakeRemoteDesktop **remote,
                          FakeInputCapture **input, FakeSecret **secret,
                          QString *error)
{
    // AGENT-GUARD: Owning these names before the frontend starts prevents the
    // private bus from activating installed host helpers during this proof.
    *chooser = new FakeFileChooser(&root);
    *screenshot = new FakeScreenshot(&root);
    *screencast = new FakeScreenCast(&root);
    *remote = new FakeRemoteDesktop(&root);
    *input = new FakeInputCapture(&root);
    *secret = new FakeSecret(&root);
    new FakeAccess(&root);
    if (!bus.registerObject(QString::fromLatin1(kPortalObjectPath), &root,
                            QDBusConnection::ExportAdaptors)
        || !bus.registerService(QStringLiteral(
                                    "org.freedesktop.impl.portal.desktop.kde"))
        || !bus.registerService(QStringLiteral(
                                    "org.freedesktop.impl.portal.desktop.gnome-keyring"))
        || !bus.registerService(QString::fromLatin1(DocumentsService))
        || !bus.registerService(QString::fromLatin1(PermissionStoreService))) {
        *error = QStringLiteral("cannot register injected private-bus routing backends");
        return false;
    }
    return true;
}

QDBusPendingCall callFrontend(QDBusConnection &bus, const char *interfaceName,
                              const char *member, QVariantList arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(FrontendService), QString::fromLatin1(kPortalObjectPath),
        QString::fromLatin1(interfaceName), QString::fromLatin1(member));
    message.setArguments(arguments);
    return bus.asyncCall(message, 5'000);
}

bool requireExported(const QString &interfaceName, QString *error)
{
    if (!waitUntil([&] { return interfaceExported(interfaceName); }, 5'000)) {
        *error = QStringLiteral("frontend did not export %1").arg(interfaceName);
        return false;
    }
    return true;
}

bool requireMissingInterface(const QString &interfaceName, QString *error)
{
    // Settle wait: the frontend finishes every export before acquiring its
    // name, and startCore waited for exactly that, so this only bounds
    // scheduler noise around the introspection observation.
    waitUntil([] { return false; }, 500);
    if (interfaceExported(interfaceName)) {
        *error = QStringLiteral("frontend unexpectedly exported %1").arg(interfaceName);
        return false;
    }
    return true;
}

QVariantMap optionsWithToken(const char *token)
{
    return QVariantMap{{QStringLiteral("handle_token"), QString::fromLatin1(token)}};
}

// AGENT-GUARD: CreateSession families require session_handle_token; 1.20.4
// xdp_session_initable_init fails the whole call with "Missing token" before
// any backend is contacted when it is absent.
QVariantMap sessionOptionsWithToken(const char *token)
{
    const QString tokenText = QString::fromLatin1(token);
    return QVariantMap{{QStringLiteral("handle_token"), tokenText},
                       {QStringLiteral("session_handle_token"), tokenText + QStringLiteral("_session")}};
}

bool failWithFrontendLog(QProcess *frontend, const QString &message, QString *error)
{
    // The frontend runs with --verbose; its stderr carries the one-line
    // g_debug/g_warning answer when a forwarded call never reaches a backend.
    *error = QStringLiteral("%1 | frontend stderr: %2")
                 .arg(message, QString::fromUtf8(frontend->readAllStandardError()));
    return false;
}

bool failWithCallReply(QProcess *frontend, QDBusPendingCall &pending,
                       const QString &message, QString *error)
{
    // The frontend answers every caller before forwarding to the backend, so
    // a refusal that never reached the fake is written on this reply.
    const QDBusMessage reply = pending.reply();
    QString replyText;
    if (reply.type() == QDBusMessage::ErrorMessage) {
        replyText = reply.errorName() + QStringLiteral(": ") + reply.errorMessage();
    }
    return failWithFrontendLog(frontend,
                               message + QStringLiteral(" | caller reply: ") + replyText,
                               error);
}

bool secretCallArguments(QVariantList &arguments, QString *error)
{
    int pipeFds[2];
    if (::pipe(pipeFds) != 0) {
        *error = QStringLiteral("cannot create the RetrieveSecret pipe");
        return false;
    }
    ::close(pipeFds[0]);
    // QDBusUnixFileDescriptor dups the descriptor, so closing the raw write
    // end here keeps exactly one owned copy in the outgoing message.
    QDBusUnixFileDescriptor writeEnd(pipeFds[1]);
    ::close(pipeFds[1]);
    if (!writeEnd.isValid()) {
        *error = QStringLiteral("cannot pass a valid RetrieveSecret descriptor");
        return false;
    }
    arguments << QVariant::fromValue(writeEnd) << optionsWithToken("qt_Routing_Secret");
    return true;
}

bool runRouting(QString *error)
{
    auto runtime = stageRoutingRuntime(/*withSecretRow=*/true, error);
    if (!runtime.has_value()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePortalRoot root;
    FakeFileChooser *chooser = nullptr;
    FakeScreenshot *screenshot = nullptr;
    FakeScreenCast *screencast = nullptr;
    FakeRemoteDesktop *remote = nullptr;
    FakeInputCapture *input = nullptr;
    FakeSecret *secret = nullptr;
    if (!registerRoutingFakes(bus, root, &chooser, &screenshot, &screencast,
                              &remote, &input, &secret, error)) {
        return false;
    }
    ChildProcesses children;
    QProcess *frontend = nullptr;
    if (!startCore(*runtime, children, &frontend, error)
        || !verifyFrontendAppearance(error)) {
        return false;
    }

    for (const char *interface : {"org.freedesktop.portal.FileChooser",
                                  "org.freedesktop.portal.Screenshot",
                                  "org.freedesktop.portal.ScreenCast",
                                  "org.freedesktop.portal.RemoteDesktop",
                                  "org.freedesktop.portal.InputCapture",
                                  "org.freedesktop.portal.Secret",
                                  "org.freedesktop.portal.Clipboard",
                                  "org.freedesktop.portal.Account",
                                  "org.freedesktop.portal.DynamicLauncher"}) {
        if (!requireExported(QString::fromLatin1(interface), error)) {
            return false;
        }
    }
    if (!requireMissingInterface(QStringLiteral("org.freedesktop.portal.Wallpaper"), error)
        || !requireMissingInterface(QStringLiteral("org.freedesktop.portal.Background"),
                                    error)) {
        return false;
    }

    QDBusPendingCall pending = callFrontend(
        bus, "org.freedesktop.portal.FileChooser", "OpenFile",
        {QString{}, QStringLiteral("QindaQt routing proof"),
         optionsWithToken("qt_Routing_OpenFile")});
    if (!waitUntil([&] { return chooser->calls() == 1; }, 5'000)
        || chooser->lastTitle() != QStringLiteral("QindaQt routing proof")) {
        return failWithCallReply(
            frontend, pending,
            QStringLiteral("FileChooser did not reach the routed kde backend"), error);
    }
    Q_UNUSED(pending)

    pending = callFrontend(bus, "org.freedesktop.portal.Screenshot", "Screenshot",
                           {QString{}, optionsWithToken("qt_Routing_Screenshot")});
    if (!waitUntil([&] { return screenshot->calls() == 1; }, 5'000)) {
        return failWithCallReply(
            frontend, pending,
            QStringLiteral("Screenshot did not reach the routed kde backend"), error);
    }

    pending = callFrontend(bus, "org.freedesktop.portal.ScreenCast", "CreateSession",
                           {sessionOptionsWithToken("qt_Routing_ScreenCast")});
    if (!waitUntil([&] { return screencast->calls() == 1; }, 5'000)) {
        return failWithCallReply(
            frontend, pending,
            QStringLiteral("ScreenCast did not reach the routed kde backend"), error);
    }

    pending = callFrontend(bus, "org.freedesktop.portal.RemoteDesktop", "CreateSession",
                           {sessionOptionsWithToken("qt_Routing_RemoteDesktop")});
    if (!waitUntil([&] { return remote->calls() == 1; }, 5'000)) {
        return failWithCallReply(
            frontend, pending,
            QStringLiteral("RemoteDesktop did not reach the routed kde backend"), error);
    }

    pending = callFrontend(bus, "org.freedesktop.portal.InputCapture", "CreateSession",
                           {QString{}, sessionOptionsWithToken("qt_Routing_InputCapture")});
    if (!waitUntil([&] { return input->calls() == 1; }, 5'000)) {
        return failWithCallReply(
            frontend, pending,
            QStringLiteral("InputCapture did not reach the routed kde backend"), error);
    }

    QVariantList retrieveArguments;
    if (!secretCallArguments(retrieveArguments, error)) {
        return false;
    }
    pending = callFrontend(bus, "org.freedesktop.portal.Secret", "RetrieveSecret",
                           retrieveArguments);
    if (!waitUntil([&] { return secret->calls() == 1; }, 5'000)) {
        return failWithCallReply(
            frontend, pending,
            QStringLiteral("Secret did not reach the routed gnome-keyring backend"),
            error);
    }
    return true;
}

bool runRoutingNegative(QString *error)
{
    auto runtime = stageRoutingRuntime(/*withSecretRow=*/false, error);
    if (!runtime.has_value()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakePortalRoot root;
    FakeFileChooser *chooser = nullptr;
    FakeScreenshot *screenshot = nullptr;
    FakeScreenCast *screencast = nullptr;
    FakeRemoteDesktop *remote = nullptr;
    FakeInputCapture *input = nullptr;
    FakeSecret *secret = nullptr;
    if (!registerRoutingFakes(bus, root, &chooser, &screenshot, &screencast,
                              &remote, &input, &secret, error)) {
        return false;
    }
    ChildProcesses children;
    QProcess *frontend = nullptr;
    if (!startCore(*runtime, children, &frontend, error)
        || !verifyFrontendAppearance(error)) {
        return false;
    }

    // Controls: the mutation removed only the Secret row.
    if (!requireMissingInterface(QStringLiteral("org.freedesktop.portal.Secret"), error)
        || !requireExported(QStringLiteral("org.freedesktop.portal.ScreenCast"), error)
        || !requireMissingInterface(QStringLiteral("org.freedesktop.portal.Wallpaper"),
                                    error)) {
        return false;
    }
    QVariantList retrieveArguments;
    if (!secretCallArguments(retrieveArguments, error)) {
        return false;
    }
    const QDBusMessage refused = frontendPortalCall(
        QStringLiteral("org.freedesktop.portal.Secret"),
        QStringLiteral("RetrieveSecret"), retrieveArguments);
    if (refused.type() != QDBusMessage::ErrorMessage) {
        *error = QStringLiteral("Secret escaped default=none without its routing row");
        return false;
    }
    if (secret->calls() != 0) {
        *error = QStringLiteral("the closed Secret route still reached a backend");
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
        qCritical("portal routing test requires one private-bus mode argument");
        return 2;
    }
    QString error;
    const QString mode = application.arguments().at(1);
    const bool passed = mode == QStringLiteral("routing")
        ? runRouting(&error)
        : mode == QStringLiteral("routing-negative") ? runRoutingNegative(&error)
                                                     : false;
    if (!passed) {
        qCritical().noquote() << (error.isEmpty() ? QStringLiteral("unknown test mode") : error);
        return 1;
    }
    return 0;
}

#include "tst_portal_frontend_routing.moc"
