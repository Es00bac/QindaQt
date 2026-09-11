// SPDX-License-Identifier: GPL-3.0-or-later
#include "portal_frontend_test_support.h"

#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/resident_portal_service.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <cmath>

using QindaQt::Services::SettingsProtocol::WireContract;
using namespace QindaQt::Services::Portal;

namespace QindaQt::Tests::Portal {
namespace {

bool writeFile(const QString &path, const QByteArray &contents, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(contents) != contents.size()) {
        *error = QStringLiteral("cannot write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool copyFile(const QString &source, const QString &destination, QString *error)
{
    if (!QFileInfo::exists(source) || QFileInfo::exists(destination)
        || !QFile::copy(source, destination)) {
        *error = QStringLiteral("cannot stage %1 as %2").arg(source, destination);
        return false;
    }
    return true;
}

QDBusMessage settingsCall(const QString &member,
                          const QVariantList &arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(WireContract::ServiceName),
        QString::fromLatin1(WireContract::ObjectPath),
        QString::fromLatin1(WireContract::InterfaceName), member);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, 5'000);
}

} // namespace

QString configuredPath(const char *environmentName, const char *fallback)
{
    const QString override = qEnvironmentVariable(environmentName);
    return override.isEmpty() ? QString::fromUtf8(fallback) : override;
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMilliseconds)
{
    QElapsedTimer timeout;
    timeout.start();
    while (timeout.elapsed() < timeoutMilliseconds) {
        if (condition()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return condition();
}

ChildProcesses::~ChildProcesses()
{
    for (auto it = m_processes.rbegin(); it != m_processes.rend(); ++it) {
        stop(**it);
    }
}

QProcess *ChildProcesses::start(const QString &program,
                                const QStringList &arguments,
                                const QProcessEnvironment &environment,
                                QString *error)
{
    auto process = std::make_unique<QProcess>();
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->start(program, arguments);
    if (!process->waitForStarted(5'000)) {
        *error = QStringLiteral("cannot start %1: %2")
                     .arg(program, process->errorString());
        return nullptr;
    }
    QProcess *result = process.get();
    m_processes.push_back(std::move(process));
    return result;
}

void ChildProcesses::stop(QProcess &process)
{
    if (process.state() == QProcess::NotRunning) {
        return;
    }
    process.terminate();
    if (!process.waitForFinished(3'000)) {
        process.kill();
        process.waitForFinished(3'000);
    }
}

bool commitColorScheme(const QString &scheme, QString *error)
{
    const QDBusMessage snapshot = settingsCall(
        QString::fromLatin1(WireContract::GetSnapshotMethod),
        {QStringList{QString::fromLatin1(
            Services::Portal::kColorSchemeSetting)}});
    if (snapshot.type() != QDBusMessage::ReplyMessage
        || snapshot.arguments().size() != 1) {
        *error = QStringLiteral("Settings1 snapshot failed: %1")
                     .arg(snapshot.errorMessage());
        return false;
    }
    const QVariantMap state = qdbus_cast<QVariantMap>(snapshot.arguments().first());
    const QString epoch = state.value(
        QString::fromLatin1(WireContract::FieldEpoch)).toString();
    const quint64 revision = state.value(
        QString::fromLatin1(WireContract::FieldRevision)).toULongLong();
    const QVariantMap operation{
        {QString::fromLatin1(WireContract::FieldKey),
         QString::fromLatin1(Services::Portal::kColorSchemeSetting)},
        {QString::fromLatin1(WireContract::FieldKind),
         QString::fromLatin1(WireContract::OperationKindSet)},
        {QString::fromLatin1(WireContract::FieldValue), scheme}};
    const QDBusMessage commit = settingsCall(
        QString::fromLatin1(WireContract::CommitUserTransactionMethod),
        {epoch, QVariant::fromValue(revision),
         QVariantList{QVariant::fromValue(operation)}});
    const QVariantMap outcome = commit.arguments().isEmpty()
        ? QVariantMap{}
        : qdbus_cast<QVariantMap>(commit.arguments().first());
    if (commit.type() != QDBusMessage::ReplyMessage
        || commit.arguments().size() != 1
        || outcome.value(QString::fromLatin1(WireContract::FieldStatus)).toUInt() != 0
        || outcome.value(QString::fromLatin1(WireContract::FieldRevisionAfter))
               .toULongLong() <= revision) {
        *error = QStringLiteral("Settings1 color-scheme commit failed: %1 status=%2")
                     .arg(commit.errorMessage())
                     .arg(outcome.value(
                              QString::fromLatin1(WireContract::FieldStatus))
                              .toUInt());
        return false;
    }
    return true;
}

std::optional<Runtime> stageRuntime(QString *error)
{
    const QString parent = QString::fromUtf8(QINDAQT_PORTAL_TEST_RUNTIME_ROOT);
    if (!QDir().mkpath(parent)) {
        *error = QStringLiteral("cannot create build-local runtime parent");
        return std::nullopt;
    }
    auto root = std::make_unique<QTemporaryDir>(
        parent + QStringLiteral("/frontend-XXXXXX"));
    if (!root->isValid()) {
        *error = QStringLiteral("cannot create build-local portal runtime");
        return std::nullopt;
    }
    const QString portals = QDir(root->path()).filePath(QStringLiteral("portals"));
    const QString config = QDir(root->path()).filePath(QStringLiteral("config"));
    const QString data = QDir(root->path()).filePath(QStringLiteral("data"));
    const QString cache = QDir(root->path()).filePath(QStringLiteral("cache"));
    const QString runtimeDirectory =
        QDir(root->path()).filePath(QStringLiteral("runtime"));
    if (!QDir().mkpath(portals) || !QDir().mkpath(config)
        || !QDir().mkpath(data) || !QDir().mkpath(cache)
        || !QDir().mkpath(runtimeDirectory)
        || !QFile::setPermissions(runtimeDirectory,
                                  QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner)
        || !copyFile(configuredPath("QINDAQT_TEST_PORTAL_METADATA",
                                    QINDAQT_PORTAL_METADATA),
                     QDir(portals).filePath(QStringLiteral("qindaqt.portal")), error)
        || !copyFile(configuredPath("QINDAQT_TEST_PORTAL_SELECTION",
                                    QINDAQT_PORTAL_SELECTION),
                     QDir(portals).filePath(
                         QStringLiteral("qindaqt-portals.conf")), error)
        || !copyFile(QString::fromUtf8(QINDAQT_FALLBACK_PORTAL),
                     QDir(portals).filePath(QStringLiteral("kde.portal")), error)
        || !writeFile(QDir(portals).filePath(QStringLiteral("portals.conf")),
                      QByteArrayLiteral("[preferred]\ndefault=none\n"), error)) {
        return std::nullopt;
    }
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
    environment.insert(QStringLiteral("XDG_DATA_HOME"), data);
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), cache);
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDirectory);
    environment.insert(QStringLiteral("XDG_CURRENT_DESKTOP"),
                       QStringLiteral("qindaqt"));
    environment.insert(QStringLiteral("XDG_DESKTOP_PORTAL_DIR"), portals);
    environment.insert(QStringLiteral("QINDAQT_SETTINGS_SCHEMA_DIR"),
                       configuredPath("QINDAQT_TEST_SETTINGS_SCHEMA_DIR",
                                      QINDAQT_SETTINGS_SCHEMA_DIR));
    environment.insert(QStringLiteral("QINDAQT_PORTAL_THEME_DIRS"),
                       configuredPath("QINDAQT_TEST_PORTAL_THEME_DIR",
                                      QINDAQT_PORTAL_THEME_DIR));
    environment.remove(QStringLiteral("DISPLAY"));
    environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
    return Runtime{.root = std::move(root),
                   .portalDirectory = portals,
                   .environment = environment};
}

bool waitForService(const QString &name)
{
    return waitUntil([&] {
        const auto reply = QDBusConnection::sessionBus().interface()
                               ->isServiceRegistered(name);
        return reply.isValid() && reply.value();
    }, 8'000);
}

bool waitForServiceGone(const QString &name)
{
    return waitUntil([&] {
        const auto reply = QDBusConnection::sessionBus().interface()
                               ->isServiceRegistered(name);
        return reply.isValid() && !reply.value();
    }, 8'000);
}

bool startCore(Runtime &runtime, ChildProcesses &children,
               QProcess **frontend, QString *error)
{
    QProcess *settings = children.start(
        configuredPath("QINDAQT_TEST_SETTINGS_EXECUTABLE",
                       QINDAQT_SETTINGS_EXECUTABLE),
        {}, runtime.environment, error);
    if (settings == nullptr
        || !waitForService(QString::fromLatin1(WireContract::ServiceName))
        || !commitColorScheme(QStringLiteral("dark"), error)) {
        return false;
    }
    QProcess *backend = children.start(
        configuredPath("QINDAQT_TEST_PORTAL_EXECUTABLE",
                       QINDAQT_PORTAL_EXECUTABLE),
        {}, runtime.environment, error);
    if (backend == nullptr
        || !waitForService(QString::fromLatin1(
            Services::Portal::kPortalServiceName))) {
        *error = QStringLiteral("QindaQt portal backend did not acquire its name");
        return false;
    }
    *frontend = children.start(QString::fromUtf8(QINDAQT_XDG_DESKTOP_PORTAL),
                               {QStringLiteral("--verbose")},
                               runtime.environment, error);
    if (*frontend == nullptr
        || !waitForService(QString::fromLatin1(FrontendService))) {
        *error = QStringLiteral("xdg-desktop-portal did not acquire its private name");
        return false;
    }
    return true;
}

namespace {

// AGENT-GUARD: These declarations are the only providers inside a routing
// runtime. The fake `kde` portal deliberately advertises Access (the 1.20.4
// frontend exports Screenshot only when Access resolves), Wallpaper and
// Background (which the closed routing rows must keep unexported), and the
// fake `gnome-keyring` portal advertises FileChooser so the proof shows
// routing exclusivity rather than mere availability.
constexpr auto kFakeKdePortal =
    "[portal]\n"
    "DBusName=org.freedesktop.impl.portal.desktop.kde\n"
    "Interfaces=org.freedesktop.impl.portal.Access;"
    "org.freedesktop.impl.portal.FileChooser;"
    "org.freedesktop.impl.portal.Screenshot;"
    "org.freedesktop.impl.portal.ScreenCast;"
    "org.freedesktop.impl.portal.RemoteDesktop;"
    "org.freedesktop.impl.portal.InputCapture;"
    "org.freedesktop.impl.portal.Clipboard;"
    "org.freedesktop.impl.portal.Usb;"
    "org.freedesktop.impl.portal.Account;"
    "org.freedesktop.impl.portal.DynamicLauncher;"
    "org.freedesktop.impl.portal.Wallpaper;"
    "org.freedesktop.impl.portal.Background\n"
    "UseIn=QindaQt\n";
constexpr auto kFakeGnomeKeyringPortal =
    "[portal]\n"
    "DBusName=org.freedesktop.impl.portal.desktop.gnome-keyring\n"
    "Interfaces=org.freedesktop.impl.portal.Secret;"
    "org.freedesktop.impl.portal.FileChooser\n"
    "UseIn=QindaQt\n";

} // namespace

std::optional<Runtime> stageRoutingRuntime(bool withSecretRow, QString *error)
{
    const QString parent = QString::fromUtf8(QINDAQT_PORTAL_TEST_RUNTIME_ROOT);
    if (!QDir().mkpath(parent)) {
        *error = QStringLiteral("cannot create build-local routing parent");
        return std::nullopt;
    }
    auto root = std::make_unique<QTemporaryDir>(
        parent + QStringLiteral("/routing-XXXXXX"));
    if (!root->isValid()) {
        *error = QStringLiteral("cannot create build-local routing runtime");
        return std::nullopt;
    }
    const QString portals = QDir(root->path()).filePath(QStringLiteral("portals"));
    const QString config = QDir(root->path()).filePath(QStringLiteral("config"));
    const QString data = QDir(root->path()).filePath(QStringLiteral("data"));
    const QString cache = QDir(root->path()).filePath(QStringLiteral("cache"));
    const QString runtimeDirectory =
        QDir(root->path()).filePath(QStringLiteral("runtime"));
    if (!QDir().mkpath(portals) || !QDir().mkpath(config)
        || !QDir().mkpath(data) || !QDir().mkpath(cache)
        || !QDir().mkpath(runtimeDirectory)
        || !QFile::setPermissions(runtimeDirectory,
                                  QFileDevice::ReadOwner
                                      | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner)
        || !copyFile(configuredPath("QINDAQT_TEST_PORTAL_METADATA",
                                    QINDAQT_PORTAL_METADATA),
                     QDir(portals).filePath(QStringLiteral("qindaqt.portal")), error)
        || !writeFile(QDir(portals).filePath(QStringLiteral("kde.portal")),
                      kFakeKdePortal, error)
        || !writeFile(QDir(portals).filePath(QStringLiteral("gnome-keyring.portal")),
                      kFakeGnomeKeyringPortal, error)
        || !writeFile(QDir(portals).filePath(QStringLiteral("portals.conf")),
                      QByteArrayLiteral("[preferred]\ndefault=none\n"), error)) {
        return std::nullopt;
    }
    QFile selection(configuredPath("QINDAQT_TEST_PORTAL_SELECTION",
                                   QINDAQT_PORTAL_SELECTION));
    if (!selection.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("cannot read routing source %1")
                     .arg(selection.fileName());
        return std::nullopt;
    }
    QStringList rows = QString::fromUtf8(selection.readAll())
                           .split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (!withSecretRow) {
        rows.removeIf([](const QString &row) {
            return row.startsWith(
                QStringLiteral("org.freedesktop.impl.portal.Secret="));
        });
    }
    if (!writeFile(QDir(portals).filePath(QStringLiteral("qindaqt-portals.conf")),
                   rows.join(QLatin1Char('\n')).toUtf8(), error)) {
        return std::nullopt;
    }
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
    environment.insert(QStringLiteral("XDG_DATA_HOME"), data);
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), cache);
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDirectory);
    environment.insert(QStringLiteral("XDG_CURRENT_DESKTOP"),
                       QStringLiteral("qindaqt"));
    environment.insert(QStringLiteral("XDG_DESKTOP_PORTAL_DIR"), portals);
    environment.insert(QStringLiteral("QINDAQT_SETTINGS_SCHEMA_DIR"),
                       configuredPath("QINDAQT_TEST_SETTINGS_SCHEMA_DIR",
                                      QINDAQT_SETTINGS_SCHEMA_DIR));
    environment.insert(QStringLiteral("QINDAQT_PORTAL_THEME_DIRS"),
                       configuredPath("QINDAQT_TEST_PORTAL_THEME_DIR",
                                      QINDAQT_PORTAL_THEME_DIR));
    environment.remove(QStringLiteral("DISPLAY"));
    environment.remove(QStringLiteral("WAYLAND_DISPLAY"));
    return Runtime{.root = std::move(root),
                   .portalDirectory = portals,
                   .environment = environment};
}

QDBusMessage frontendPortalCall(const QString &interfaceName,
                                const QString &member,
                                const QVariantList &arguments,
                                int timeoutMilliseconds)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(FrontendService),
        QString::fromLatin1(kPortalObjectPath), interfaceName, member);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block,
                                              timeoutMilliseconds);
}

bool interfaceExported(const QString &interfaceName)
{
    // The 1.20.4 frontend exports every resolved interface before it acquires
    // org.freedesktop.portal.Desktop, so a completed name acquisition implies
    // the introspection answer is final.
    QDBusMessage introspect = QDBusMessage::createMethodCall(
        QString::fromLatin1(FrontendService),
        QString::fromLatin1(kPortalObjectPath),
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QStringLiteral("Introspect"));
    const QDBusMessage reply = QDBusConnection::sessionBus().call(
        introspect, QDBus::Block, 5'000);
    return reply.type() == QDBusMessage::ReplyMessage
        && reply.arguments().size() == 1
        && reply.arguments().first().toString().contains(interfaceName);
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

namespace {

using PortalNamespaceMap = QMap<QString, QVariantMap>;

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
    // Smoked Plum publishes its opaque #EAB391 QST accent over the real bus.
    if (!near(red, 234.0 / 255.0) || !near(green, 179.0 / 255.0)
        || !near(blue, 145.0 / 255.0)) {
        *error = QStringLiteral("frontend accent-color differs from QindaQt QST projection");
        return false;
    }
    return true;
}

} // namespace

bool verifyFrontendAppearance(QString *error)
{
    const QDBusMessage all = frontendPortalCall(
        QString::fromLatin1(FrontendInterface), QStringLiteral("ReadAll"),
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
        const QDBusMessage one = frontendPortalCall(
            QString::fromLatin1(FrontendInterface), QStringLiteral("Read"),
            {QString::fromLatin1(kAppearanceNamespace), key});
        if (one.type() != QDBusMessage::ReplyMessage
            || one.arguments().size() != 1) {
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

} // namespace QindaQt::Tests::Portal
