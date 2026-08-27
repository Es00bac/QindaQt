// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

using namespace QindaQt::Audio;

namespace
{

class ProcessGuard final
{
public:
    ~ProcessGuard()
    {
        stop();
    }

    bool start(const QProcessEnvironment &environment)
    {
        process.setProcessEnvironment(environment);
        process.setProgram(QStringLiteral(QINDAQT_PIPEWIRE_EXECUTABLE));
        process.setArguments({QStringLiteral("-c"),
                              QStringLiteral(QINDAQT_PIPEWIRE_TEST_CONFIG)});
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start();
        return process.waitForStarted(5000);
    }

    void stop()
    {
        if (process.state() == QProcess::NotRunning) {
            return;
        }
        process.terminate();
        if (!process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished(3000);
        }
    }

    QProcess process;
};

class SnapshotLog final
{
public:
    void append(Snapshot snapshot)
    {
        std::lock_guard lock(mutex);
        values.push_back(std::move(snapshot));
        changed.notify_all();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard lock(mutex);
        return values.size();
    }

    [[nodiscard]] std::optional<Snapshot> waitForReason(
        const QString &reasonCode, const std::size_t after,
        const std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(mutex);
        const auto find = [&]() -> std::optional<Snapshot> {
            for (std::size_t index = after; index < values.size(); ++index) {
                if (values[index].reasonCode == reasonCode) {
                    return values[index];
                }
            }
            return std::nullopt;
        };
        if (!changed.wait_for(lock, timeout, [&] { return find().has_value(); })) {
            return std::nullopt;
        }
        return find();
    }

private:
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::vector<Snapshot> values;
};

class ResetScheduler final
{
public:
    void armBlockingReset()
    {
        std::lock_guard lock(mutex);
        blockNext = true;
        release = false;
        stopQueued = false;
    }

    void resetScheduled()
    {
        std::unique_lock lock(mutex);
        ++scheduledCount;
        const bool shouldBlock = std::exchange(blockNext, false);
        changed.notify_all();
        if (shouldBlock) {
            changed.wait(lock, [&] { return release; });
        }
    }

    void stopTaskQueued()
    {
        std::lock_guard lock(mutex);
        stopQueued = true;
        changed.notify_all();
    }

    [[nodiscard]] bool waitForScheduled(const int expected)
    {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(5),
                                [&] { return scheduledCount >= expected; });
    }

    [[nodiscard]] bool waitForStopTask()
    {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(5),
                                [&] { return stopQueued; });
    }

    void releaseReset()
    {
        std::lock_guard lock(mutex);
        release = true;
        changed.notify_all();
    }

private:
    std::mutex mutex;
    std::condition_variable changed;
    int scheduledCount = 0;
    bool blockNext = false;
    bool release = false;
    bool stopQueued = false;
};

qsizetype openFileDescriptorCount()
{
    return QDir(QStringLiteral("/proc/self/fd"))
        .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
        .size();
}

} // namespace

class WirePlumberResetLifecycleTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stoppedResetSourceCannotPoisonRestart();
};

void WirePlumberResetLifecycleTests::stoppedResetSourceCannotPoisonRestart()
{
    QTemporaryDir root(QStringLiteral("/tmp/qindaqt-audio-reset-XXXXXX"));
    QVERIFY(root.isValid());
    const QString runtime = root.filePath(QStringLiteral("runtime"));
    const QString state = root.filePath(QStringLiteral("state"));
    const QString config = root.filePath(QStringLiteral("config"));
    QVERIFY(QDir().mkpath(runtime));
    QVERIFY(QDir().mkpath(state));
    QVERIFY(QDir().mkpath(config));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
    environment.insert(QStringLiteral("PIPEWIRE_RUNTIME_DIR"), runtime);
    environment.insert(QStringLiteral("XDG_STATE_HOME"), state);
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
    environment.insert(
        QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
        QStringLiteral("unix:path=%1/no-session-bus").arg(root.path()));
    environment.remove(QStringLiteral("PIPEWIRE_REMOTE"));
    qputenv("XDG_RUNTIME_DIR", runtime.toUtf8());
    qputenv("PIPEWIRE_RUNTIME_DIR", runtime.toUtf8());
    qputenv("XDG_STATE_HOME", state.toUtf8());
    qputenv("XDG_CONFIG_HOME", config.toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS",
            environment.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS")).toUtf8());
    qunsetenv("PIPEWIRE_REMOTE");

    SnapshotLog snapshots;
    ResetScheduler scheduler;
    const qsizetype descriptorsBefore = openFileDescriptorCount();
    WirePlumberWorker worker(
        100, [&snapshots](Snapshot snapshot) { snapshots.append(std::move(snapshot)); },
        [](quint64, BackendOperationOutcome) {},
        {.disconnectResetScheduled = [&scheduler] { scheduler.resetScheduled(); },
         .stopTaskQueued = [&scheduler] { scheduler.stopTaskQueued(); }});

    for (int cycle = 0; cycle < 2; ++cycle) {
        ProcessGuard firstPipeWire;
        QVERIFY2(firstPipeWire.start(environment),
                 qPrintable(QString::fromUtf8(firstPipeWire.process.readAll())));
        QTRY_VERIFY_WITH_TIMEOUT(
            QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")), 5000);
        std::size_t baseline = snapshots.size();
        worker.start();
        const auto firstConnected = snapshots.waitForReason(
            QStringLiteral("wireplumber-unavailable"), baseline,
            std::chrono::seconds(5));
        if (!firstConnected.has_value()) {
            worker.stop();
        }
        QVERIFY(firstConnected.has_value());

        scheduler.armBlockingReset();
        firstPipeWire.stop();
        const int firstLossNumber = cycle * 2 + 1;
        const bool firstResetScheduled = scheduler.waitForScheduled(firstLossNumber);
        if (!firstResetScheduled) {
            worker.stop();
        }
        QVERIFY(firstResetScheduled);

        std::thread stopper([&worker] { worker.stop(); });
        const bool stopWasQueued = scheduler.waitForStopTask();
        scheduler.releaseReset();
        stopper.join();
        QVERIFY(stopWasQueued);

        ProcessGuard secondPipeWire;
        QVERIFY2(secondPipeWire.start(environment),
                 qPrintable(QString::fromUtf8(secondPipeWire.process.readAll())));
        QTRY_VERIFY_WITH_TIMEOUT(
            QFileInfo::exists(runtime + QStringLiteral("/pipewire-0")), 5000);
        baseline = snapshots.size();
        worker.start();
        const auto secondConnected = snapshots.waitForReason(
            QStringLiteral("wireplumber-unavailable"), baseline,
            std::chrono::seconds(5));
        if (!secondConnected.has_value()) {
            worker.stop();
        }
        QVERIFY(secondConnected.has_value());
        QCOMPARE(secondConnected->epoch, firstConnected->epoch + 1);

        baseline = snapshots.size();
        secondPipeWire.stop();
        const int secondLossNumber = cycle * 2 + 2;
        const bool secondResetScheduled = scheduler.waitForScheduled(secondLossNumber);
        const auto secondLoss = snapshots.waitForReason(
            QStringLiteral("pipewire-unavailable"), baseline,
            std::chrono::seconds(5));
        worker.stop();
        QVERIFY(secondResetScheduled);
        QVERIFY(secondLoss.has_value());
        QCOMPARE(secondLoss->epoch, secondConnected->epoch + 1);
    }

    const qsizetype descriptorsAfter = openFileDescriptorCount();
    QVERIFY2(descriptorsAfter <= descriptorsBefore + 5,
             qPrintable(QStringLiteral("FD growth after reset cycles: %1 -> %2")
                            .arg(descriptorsBefore)
                            .arg(descriptorsAfter)));
}

QTEST_GUILESS_MAIN(WirePlumberResetLifecycleTests)
#include "tst_wireplumber_reset_lifecycle.moc"
