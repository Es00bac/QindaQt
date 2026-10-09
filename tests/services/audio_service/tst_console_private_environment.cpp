// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/gain_native_graph.h"
#include "support/gain_private_runtime.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QTemporaryDir>
#include <QtTest>
#include <optional>

using namespace QindaQt::Audio::Test;

namespace
{
class HostileEnvironment final
{
public:
    void set(const QByteArray &key, const QByteArray &value)
    {
        previous.insert(key, qEnvironmentVariableIsSet(key.constData())
            ? std::optional<QByteArray>{qgetenv(key.constData())} : std::nullopt);
        qputenv(key.constData(), value);
    }
    ~HostileEnvironment()
    {
        for (auto it = previous.cbegin(); it != previous.cend(); ++it)
            if (it.value()) qputenv(it.key().constData(), *it.value());
            else qunsetenv(it.key().constData());
    }
private:
    QMap<QByteArray, std::optional<QByteArray>> previous;
};
}
class ConsolePrivateEnvironmentTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void hostileSocketConfigAndNotificationOverridesCannotEscape();
    void ambientModuleOverrideRefusesBeforePrivateStartup();
};
void ConsolePrivateEnvironmentTests::hostileSocketConfigAndNotificationOverridesCannotEscape()
{
    QTemporaryDir other(QStringLiteral("/tmp/qindaqt-gain-hostile-XXXXXX")); QVERIFY(other.isValid());
    const QByteArray outside = other.filePath(QStringLiteral("redirected-socket")).toUtf8();
    HostileEnvironment environment;
    environment.set("PIPEWIRE_CORE", outside);
    environment.set("PIPEWIRE_REMOTE", outside);
    environment.set("PIPEWIRE_CONFIG_DIR", other.path().toUtf8());
    environment.set("PIPEWIRE_CONFIG_PREFIX", other.path().toUtf8());
    environment.set("PIPEWIRE_PROPS", "{ remote.name = invalid }");
    environment.set("PIPEWIRE_NOTIFICATION_FD", "1");
    environment.set("WIREPLUMBER_CONFIG_DIR", other.path().toUtf8());
    environment.set("NOTIFY_SOCKET", outside);
    environment.set("LISTEN_FDS", "1");
    environment.set("LISTEN_PID", QByteArray::number(QCoreApplication::applicationPid()));
    {
        PrivateGainRuntime runtime; QVERIFY2(runtime.start(), qPrintable(runtime.failure()));
        QCOMPARE(qgetenv("PIPEWIRE_CORE"), QByteArray("pipewire-0"));
        QCOMPARE(qgetenv("PIPEWIRE_REMOTE"), QByteArray("pipewire-0"));
        for (const char *key : {"PIPEWIRE_PROPS", "PIPEWIRE_CONFIG_PREFIX", "PIPEWIRE_NOTIFICATION_FD",
                                "NOTIFY_SOCKET", "LISTEN_FDS", "LISTEN_PID"})
            QVERIFY(!qEnvironmentVariableIsSet(key));
        QCOMPARE(qgetenv("SPA_PLUGIN_DIR"), runtime.image().bootstrapValues().value("SPA_PLUGIN_DIR"));
        GainNativeGraph graph(runtime); QVERIFY2(graph.start(), qPrintable(graph.failure()));
        QVERIFY(graph.producer().valid());
        QVERIFY(!QFileInfo::exists(QString::fromUtf8(outside)));
    }
    QCOMPARE(qgetenv("PIPEWIRE_CORE"), outside);
    QCOMPARE(qgetenv("PIPEWIRE_REMOTE"), outside);
    QVERIFY(!QFileInfo::exists(QString::fromUtf8(outside)));
}
void ConsolePrivateEnvironmentTests::ambientModuleOverrideRefusesBeforePrivateStartup()
{
    HostileEnvironment environment;
    environment.set("PIPEWIRE_MODULE_DIR", "/absent-qindaqt-private-module-override");
    PrivateGainRuntime runtime;
    QVERIFY(!runtime.start());
    QCOMPARE(runtime.failure(), QStringLiteral("qualified-loader-cohort-refused"));
    QVERIFY(!runtime.ready());
}
QTEST_GUILESS_MAIN(ConsolePrivateEnvironmentTests)
#include "tst_console_private_environment.moc"
