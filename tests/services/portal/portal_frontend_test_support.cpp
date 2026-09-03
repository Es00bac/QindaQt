// SPDX-License-Identifier: GPL-3.0-or-later
#include "portal_frontend_test_support.h"

#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/portal/resident_portal_service.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>

using QindaQt::Services::SettingsProtocol::WireContract;

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

} // namespace QindaQt::Tests::Portal
