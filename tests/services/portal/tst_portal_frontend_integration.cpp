// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/resident_portal_service.h"
#include "portal_frontend_test_support.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>

#include <cmath>

using namespace QindaQt::Services::Portal;
using namespace QindaQt::Tests::Portal;

using PortalNamespaceMap = QMap<QString, QVariantMap>;

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

bool registerInjectedServices(QDBusConnection &bus, FakeFileChooser &fallback,
                              QString *error)
{
    // AGENT-GUARD: Owning these names before the frontend starts prevents the
    // private bus from activating installed host helpers during this proof.
    if (!bus.registerObject(QString::fromLatin1(kPortalObjectPath), &fallback,
                            QDBusConnection::ExportScriptableSlots
                                | QDBusConnection::ExportScriptableProperties)
        || !bus.registerService(QString::fromLatin1(FallbackService))
        || !bus.registerService(QString::fromLatin1(DocumentsService))
        || !bus.registerService(QString::fromLatin1(PermissionStoreService))) {
        *error = QStringLiteral("cannot register injected private-bus portal services");
        return false;
    }
    return true;
}

QDBusMessage call(const QString &interfaceName, const QString &member,
                  const QVariantList &arguments = {}, int timeout = 5'000)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(FrontendService),
        QString::fromLatin1(kPortalObjectPath), interfaceName, member);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, timeout);
}

QVariant unwrapVariant(QVariant value)
{
    for (int depth = 0;
         depth < 3 && value.metaType() == QMetaType::fromType<QDBusVariant>();
         ++depth) {
        value = qvariant_cast<QDBusVariant>(value).variant();
    }
    return value;
}

bool near(double actual, double expected)
{
    return std::abs(actual - expected) < 0.00001;
}

bool verifyAccent(const QVariant &input, QString *error)
{
    const QVariant value = unwrapVariant(input);
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) {
        *error = QStringLiteral("accent-color did not retain its (ddd) signature");
        return false;
    }
    const QDBusArgument argument = qvariant_cast<QDBusArgument>(value);
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    argument.beginStructure();
    argument >> red >> green >> blue;
    argument.endStructure();
    if (!near(red, 217.0 / 255.0) || !near(green, 138.0 / 255.0)
        || !near(blue, 50.0 / 255.0)) {
        *error = QStringLiteral("frontend accent-color differs from QindaQt QST projection");
        return false;
    }
    return true;
}

bool verifyFrontendValues(QString *error)
{
    const QDBusMessage all = call(QString::fromLatin1(FrontendInterface),
                                  QStringLiteral("ReadAll"),
                                  {QStringList{QStringLiteral("org.freedesktop.appearance")}});
    if (all.type() != QDBusMessage::ReplyMessage || all.arguments().size() != 1) {
        *error = QStringLiteral("frontend ReadAll failed: %1").arg(all.errorMessage());
        return false;
    }
    const PortalNamespaceMap namespaces =
        qdbus_cast<PortalNamespaceMap>(all.arguments().first());
    const QVariantMap appearance = namespaces.value(
        QString::fromLatin1(kAppearanceNamespace));
    const QVariant scheme = unwrapVariant(
        appearance.value(QString::fromLatin1(kColorSchemeKey)));
    const QVariant contrast = unwrapVariant(
        appearance.value(QString::fromLatin1(kContrastKey)));
    if (appearance.size() != 3
        || scheme.toUInt() != 1 || contrast.toUInt() != 0
        || !verifyAccent(appearance.value(QString::fromLatin1(kAccentColorKey)), error)) {
        if (error->isEmpty()) {
            *error = QStringLiteral(
                "frontend ReadAll values differ: namespaces=%1 keys=%2 scheme=%3 "
                "contrast=%4 schemeType=%5 contrastType=%6")
                         .arg(namespaces.size())
                         .arg(appearance.size())
                         .arg(scheme.toUInt())
                         .arg(contrast.toUInt())
                         .arg(scheme.metaType().name())
                         .arg(contrast.metaType().name());
        }
        return false;
    }
    for (const QString &key : {QString::fromLatin1(kColorSchemeKey),
                               QString::fromLatin1(kAccentColorKey),
                               QString::fromLatin1(kContrastKey)}) {
        const QDBusMessage one = call(QString::fromLatin1(FrontendInterface),
                                      QStringLiteral("Read"),
                                      {QString::fromLatin1(kAppearanceNamespace), key});
        if (one.type() != QDBusMessage::ReplyMessage || one.arguments().size() != 1) {
            *error = QStringLiteral("frontend Read failed for %1: %2")
                         .arg(key, one.errorMessage());
            return false;
        }
        const QVariant value = unwrapVariant(one.arguments().first());
        if ((key == QString::fromLatin1(kColorSchemeKey) && value.toUInt() != 1)
            || (key == QString::fromLatin1(kContrastKey) && value.toUInt() != 0)
            || (key == QString::fromLatin1(kAccentColorKey)
                && !verifyAccent(value, error))) {
            return false;
        }
    }
    return true;
}

bool runSelection(QString *error)
{
    auto runtime = stageRuntime(error);
    if (!runtime.has_value()) {
        return false;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    FakeFileChooser fallback;
    if (!registerInjectedServices(bus, fallback, error)) {
        return false;
    }
    ChildProcesses children;
    QProcess *frontend = nullptr;
    if (!startCore(*runtime, children, &frontend, error)
        || !verifyFrontendValues(error)) {
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

    const QDBusMessage background = call(
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
    const QDBusMessage negative = call(QString::fromLatin1(FrontendInterface),
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
    if (!registerInjectedServices(bus, fallback, error)) {
        return false;
    }
    ChildProcesses children;
    QProcess *frontend = nullptr;
    if (!startCore(*runtime, children, &frontend, error)
        || !verifyFrontendValues(error)) {
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
