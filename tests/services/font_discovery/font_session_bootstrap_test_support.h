// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// AGENT-NOTE: Child-process machinery for the FontSessionBootstrap rows. The
// production composition root must run BEFORE QGuiApplication construction,
// so the rows exercise it in a child process (`--qindaqt-font-bootstrap-probe`
// mode) and host canned Settings1 services in a second child mode
// (`--qindaqt-font-fake-service=<profile>`). Every bus is a private
// dbus-daemon; no host bus is ever touched (review findings P1-1/P1-4/P1-6 of
// rejected candidate abc76f3).

#include "qindaqt/services/font_discovery/font_session_bootstrap.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdio>

namespace FontSessionBootstrapTestSupport {

using QindaQt::Services::FontDiscovery::FontSessionBootstrap;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

constexpr auto ProbeMode = "--qindaqt-font-bootstrap-probe";
constexpr auto FakeServiceModePrefix = "--qindaqt-font-fake-service=";

inline QVariantMap profileValues(const QString &profile)
{
    if (profile == QLatin1String("wrong-types")) {
        // AGENT-NOTE: review finding P1-4 — the unrepaired bootstrap coerced
        // exactly these wrong-typed values into applied settings.
        return {{QStringLiteral("fonts.family"), 123},
                {QStringLiteral("fonts.monospaceFamily"), QStringLiteral("Liberation Mono")},
                {QStringLiteral("fonts.pointSize"), QStringLiteral("12.5")},
                {QStringLiteral("fonts.antialiasing"), true},
                {QStringLiteral("fonts.hinting"), QStringLiteral("full")},
                {QStringLiteral("fonts.subpixelOrder"), QStringLiteral("vrgb")}};
    }
    const QString family = profile == QLatin1String("unresolved-family")
                               ? QStringLiteral("QindaQt Missing Family XYZ")
                               : QStringLiteral("Liberation Mono");
    return {{QStringLiteral("fonts.family"), family},
            {QStringLiteral("fonts.monospaceFamily"), QStringLiteral("Liberation Mono")},
            {QStringLiteral("fonts.pointSize"), 13.5},
            {QStringLiteral("fonts.antialiasing"), true},
            {QStringLiteral("fonts.hinting"), QStringLiteral("full")},
            {QStringLiteral("fonts.subpixelOrder"), QStringLiteral("vrgb")}};
}

inline QVariantMap snapshotWireFor(const QString &profile, const QStringList &requestedKeys)
{
    const QVariantMap canned = profileValues(profile);
    QVariantMap values;
    QVariantMap sources;
    for (const QString &key : requestedKeys) {
        values.insert(key, canned.value(key));
        sources.insert(key, QStringLiteral("user-overrides"));
    }
    const quint32 wireSchema = profile == QLatin1String("bad-envelope")
                                   ? quint32(99)
                                   : WireContract::WireSchemaVersion;
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion), wireSchema},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch-fake-1")},
            {QLatin1StringView(WireContract::FieldRevision), quint64(3)},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

class FakeSettingsObject final : public QDBusVirtualObject {
public:
    explicit FakeSettingsObject(QString profile) : m_profile(std::move(profile)) {}

    QString introspect(const QString &) const override { return QString(); }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.member() == QLatin1String(WireContract::GetSnapshotMethod)
            && !message.arguments().isEmpty()) {
            connection.send(message.createReply(
                QVariant::fromValue(snapshotWireFor(m_profile,
                                                    message.arguments().constFirst().toStringList()))));
            return true;
        }
        return false;
    }

private:
    QString m_profile;
};

inline int runProbe(int argc, char **argv)
{
    QElapsedTimer timer;
    timer.start();
    QString diagnostic;
    const bool applied = FontSessionBootstrap::applyFromSessionSettings(&diagnostic);
    const qint64 readElapsed = timer.elapsed();
    QGuiApplication application(argc, argv);
    const QFont font = QGuiApplication::font();
    std::printf("applied=%d|family=%s|size=%g|hinting=%d|noantialias=%d|elapsed=%lld\n",
                applied ? 1 : 0, qPrintable(font.family()), font.pointSizeF(),
                static_cast<int>(font.hintingPreference()),
                (font.styleStrategy() & QFont::NoAntialias) != 0 ? 1 : 0,
                static_cast<long long>(readElapsed));
    std::fflush(stdout);
    return 0;
}

inline int runFakeService(int argc, char **argv, const QString &profile)
{
    QCoreApplication application(argc, argv);
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return 2;
    }
    FakeSettingsObject object(profile);
    if (!bus.registerVirtualObject(QString::fromLatin1(WireContract::ObjectPath), &object)) {
        return 3;
    }
    if (!bus.registerService(QString::fromLatin1(WireContract::ServiceName))) {
        return 4;
    }
    return QCoreApplication::exec();
}

// Private dbus-daemon fixture (same pattern as the bluetooth private-bus
// tests; the class is duplicated here because test support must not reach
// across module test trees).
class PrivateBus final {
public:
    bool start()
    {
        m_process.setProgram(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE));
        m_process.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                                QStringLiteral("--nopidfile"),
                                QStringLiteral("--print-address=1")});
        m_process.start();
        if (!m_process.waitForStarted(5'000) || !m_process.waitForReadyRead(5'000)) {
            return false;
        }
        m_address = QString::fromUtf8(m_process.readLine()).trimmed();
        m_name = QStringLiteral("qindaqt-font-bootstrap-test-%1")
                     .arg(QCoreApplication::applicationPid());
        m_connection = QDBusConnection::connectToBus(m_address, m_name);
        return !m_address.isEmpty() && m_connection.isConnected();
    }

    ~PrivateBus()
    {
        if (!m_name.isEmpty()) {
            QDBusConnection::disconnectFromBus(m_name);
        }
        m_process.terminate();
        if (!m_process.waitForFinished(1'000)) {
            m_process.kill();
            m_process.waitForFinished(1'000);
        }
    }

    [[nodiscard]] const QString &address() const noexcept { return m_address; }
    [[nodiscard]] const QDBusConnection &connection() const noexcept { return m_connection; }

private:
    QProcess m_process;
    QString m_address;
    QString m_name;
    QDBusConnection m_connection{QStringLiteral("qindaqt-font-bootstrap-unconnected")};
};

struct ProbeOutcome final {
    bool completed = false;
    bool applied = false;
    QString family;
    double pointSize = 0.0;
    int hinting = -1;
    int noAntialias = -1;
    qint64 elapsed = -1;
    QString raw;
};

[[nodiscard]] inline QProcessEnvironment baseChildEnvironment(const QString &homePath)
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("PATH"), QString::fromUtf8(qgetenv("PATH")));
    environment.insert(QStringLiteral("HOME"), homePath);
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    return environment;
}

[[nodiscard]] inline ProbeOutcome parseProbeOutput(int exitCode, const QString &output)
{
    ProbeOutcome outcome;
    outcome.raw = output;
    if (exitCode != 0) {
        return outcome;
    }
    for (const QString &field : output.trimmed().split(QLatin1Char('|'))) {
        const qsizetype equals = field.indexOf(QLatin1Char('='));
        if (equals <= 0) {
            continue;
        }
        const QString key = field.left(equals);
        const QString value = field.mid(equals + 1);
        if (key == QLatin1String("applied")) {
            outcome.applied = value == QLatin1String("1");
        } else if (key == QLatin1String("family")) {
            outcome.family = value;
        } else if (key == QLatin1String("size")) {
            outcome.pointSize = value.toDouble();
        } else if (key == QLatin1String("hinting")) {
            outcome.hinting = value.toInt();
        } else if (key == QLatin1String("noantialias")) {
            outcome.noAntialias = value.toInt();
        } else if (key == QLatin1String("elapsed")) {
            outcome.elapsed = value.toLongLong();
        }
    }
    outcome.completed = !outcome.family.isEmpty() && outcome.elapsed >= 0;
    return outcome;
}

[[nodiscard]] inline ProbeOutcome runProbeChild(const QProcessEnvironment &environment,
                                                int timeoutMilliseconds = 30'000)
{
    QProcess probe;
    probe.setProcessEnvironment(environment);
    probe.setProgram(QCoreApplication::applicationFilePath());
    probe.setArguments({QString::fromLatin1(ProbeMode)});
    probe.start();
    if (!probe.waitForFinished(timeoutMilliseconds)) {
        probe.kill();
        probe.waitForFinished(1'000);
        return {};
    }
    return parseProbeOutput(probe.exitCode(),
                            QString::fromUtf8(probe.readAllStandardOutput()));
}

// Writes a fontconfig configuration naming the vendored fixture directory and
// returns its path; discovery through productionDefault() must resolve the
// fixture families from it.
[[nodiscard]] inline QString stageFixtureFontconfig(QTemporaryDir &stage)
{
    const QString configPath = stage.filePath(QStringLiteral("fonts.conf"));
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    const QByteArray xml = QByteArrayLiteral("<?xml version=\"1.0\"?>\n<fontconfig><cachedir>")
                           + stage.filePath(QStringLiteral("cache")).toUtf8()
                           + QByteArrayLiteral("</cachedir><dir>")
                           + QByteArrayLiteral(QINDAQT_FONT_FIXTURES)
                           + QByteArrayLiteral("</dir></fontconfig>\n");
    if (file.write(xml) != xml.size()) {
        return {};
    }
    return configPath;
}

[[nodiscard]] inline bool waitForServiceOwner(const QDBusConnection &connection,
                                              int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
        call << QString::fromLatin1(WireContract::ServiceName);
        const QDBusMessage reply = connection.call(call, QDBus::Block, 1'000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()
            && reply.arguments().constFirst().toString().startsWith(QLatin1Char(':'))) {
            return true;
        }
        QTest::qSleep(50);
    }
    return false;
}

class ServiceChild final {
public:
    bool start(const QProcessEnvironment &environment, const QStringList &arguments,
               const QDBusConnection &watcherConnection)
    {
        m_process.setProcessEnvironment(environment);
        m_process.setProgram(QCoreApplication::applicationFilePath());
        m_process.setArguments(arguments);
        m_process.start();
        return m_process.waitForStarted(5'000)
               && waitForServiceOwner(watcherConnection, 10'000);
    }

    ~ServiceChild()
    {
        m_process.terminate();
        if (!m_process.waitForFinished(2'000)) {
            m_process.kill();
            m_process.waitForFinished(2'000);
        }
    }

private:
    QProcess m_process;
};

[[nodiscard]] inline QProcessEnvironment busChildEnvironment(const PrivateBus &bus,
                                                             const QString &homePath,
                                                             const QString &fontconfigFile)
{
    QProcessEnvironment environment = baseChildEnvironment(homePath);
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), bus.address());
    if (!fontconfigFile.isEmpty()) {
        environment.insert(QStringLiteral("FONTCONFIG_FILE"), fontconfigFile);
        environment.insert(QStringLiteral("FONTCONFIG_PATH"), QStringLiteral("/nonexistent"));
    }
    return environment;
}

} // namespace FontSessionBootstrapTestSupport
