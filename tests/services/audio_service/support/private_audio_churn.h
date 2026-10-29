// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/audio_service/audio_operation_coordinator.h>
#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>

#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>

#include <functional>
#include <memory>
#include <optional>

namespace QindaQt::Audio::Test
{

// glibc's in-use bytes over every arena plus mmapped chunks (0 elsewhere).
// The growth these rows guard was malloc'd heap with no new mapping or fd.
[[nodiscard]] qint64 heapInUse();
[[nodiscard]] bool hasInput(const Snapshot &snapshot, const QString &nodeName);
// Processes events until `condition` holds; false after `timeoutMs`.
[[nodiscard]] bool waitFor(const std::function<bool()> &condition, int timeoutMs);

// AGENT-GUARD: a child's output never accumulates in this process. QProcess
// keeps unread output on the heap, and the private daemons log on graph
// changes; buffered here, that log alone measured 16 kB per client stream.
class ChildProcess final
{
public:
    ~ChildProcess() { stop(); }
    bool start(const QString &program, const QStringList &arguments,
               const QProcessEnvironment &environment, const QString &logPath = {});
    void stop();

private:
    QProcess m_process;
};

// One private PipeWire/WirePlumber graph with a null output and a null input,
// and the production backend and coordinator bound to it in this process.
// Everything lives under a temporary root; the session-bus address is
// unreachable. Process environment a row changes for the backend (an empty
// LADSPA_PATH, a reduced PIPEWIRE_MODULE_DIR) must be set before start() and
// restored after destruction: the backend thread reads it with getenv.
class PrivateAudioChurn final
{
public:
    PrivateAudioChurn();
    ~PrivateAudioChurn();
    PrivateAudioChurn(const PrivateAudioChurn &) = delete;
    PrivateAudioChurn &operator=(const PrivateAudioChurn &) = delete;

    [[nodiscard]] bool start(QString *failure);
    // A console operation the coordinator applies at once; true on success.
    [[nodiscard]] bool apply(const OperationRequest &request);
    // A pw-cat playback client appears in the graph and then leaves it.
    [[nodiscard]] bool cycle(const QString &nodeName = {}, bool capture = false);
    // Runs a PipeWire tool (pw-cli, ...) against this rig's private daemon;
    // true when it exits 0 within five seconds.
    [[nodiscard]] bool runTool(const QString &program, const QStringList &arguments);
    // Returns once no publication has arrived for half a second.
    void settle();
    // Warm-up then measured cycles; the measured heap growth, if every
    // client appeared and left.
    [[nodiscard]] std::optional<qint64> churnGrowth(int warmup, int measured);
    [[nodiscard]] const Snapshot &snapshot() const;

private:
    QTemporaryDir m_root;
    QProcessEnvironment m_environment;
    ChildProcess m_pipewire;
    ChildProcess m_wireplumber;
    std::unique_ptr<WirePlumberAudioBackend> m_backend;
    std::unique_ptr<AudioOperationCoordinator> m_coordinator;
    int m_publications = 0;
    int m_clientStreams = -1;
};

// Counts the messages that contain each fragment while it lives, from any
// thread, and forwards every message to the handler it replaced (QtTest's).
// One tally at a time.
class WarningTally final
{
public:
    explicit WarningTally(QStringList fragments);
    ~WarningTally();
    WarningTally(const WarningTally &) = delete;
    WarningTally &operator=(const WarningTally &) = delete;
    [[nodiscard]] int count(const QString &fragment) const;
};

} // namespace QindaQt::Audio::Test
