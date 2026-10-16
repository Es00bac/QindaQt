// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QThread;

namespace QindaQt::Compositor {
class WineIdentityCache;
}

namespace QindaQt::Compositor::KWinIntegration {

// Resolves the program behind an opaque launcher window class (ADR-0169,
// ADR-0230) once per window instead of once per task-facts sample, and keeps
// the Wine/Steam filesystem work off the compositor thread (ADR-0384).
//
// Per (window, authenticated PID, class):
//   - the client command line is read once, synchronously, because the
//     executable is the window's identity and task facts must never publish a
//     window without one; it is cached until forget() or a PID/class change;
//   - the Steam manifest name, the environment read and the PE icon
//     extraction run as one job on a single worker thread that owns the
//     WineIdentityCache. Until it answers, identity() reports no Steam name
//     (ADR-0169's executable fallback); identityChanged() announces the late
//     part so the caller can re-sample.
//
// Threading: every public call and signal is on this object's thread. The
// worker never touches this object; results are posted back and dropped if
// this object or the window's entry is gone. Destruction waits a bounded time
// for an in-flight job (see the destructor's AGENT-GUARD).
class ForeignIdentityResolver final : public QObject
{
    Q_OBJECT

public:
    struct Identity final
    {
        QString clientExecutable;
        QString steamName;
        friend bool operator==(const Identity &, const Identity &) = default;
    };

    // Process I/O seams. Production reads bounded /proc files; tests inject
    // canned bytes. `commandLine` runs on this object's thread, `environment`
    // on the worker.
    struct ProcessReader final
    {
        std::function<QByteArray(qint64 pid)> commandLine;
        std::function<QByteArray(qint64 pid)> environment;
    };
    using Job = std::function<void()>;
    // Runs a job "elsewhere". Empty selects this object's own worker thread;
    // tests pass an inline executor to stay deterministic.
    using Executor = std::function<void(Job)>;

    // `cache` is shared with worker jobs and used only from the worker (or
    // the inline test executor).
    ForeignIdentityResolver(std::shared_ptr<WineIdentityCache> cache,
                            ProcessReader reader = {}, Executor executor = {},
                            QObject *parent = nullptr);
    ~ForeignIdentityResolver() override;

    [[nodiscard]] Identity identity(const QString &windowId, qint64 processId,
                                    const QString &resourceClass);
    void forget(const QString &windowId);
    [[nodiscard]] qsizetype commandLineReads() const noexcept { return m_commandLineReads; }

    static constexpr qsizetype MaximumTrackedWindows = 512;

Q_SIGNALS:
    void identityChanged(const QString &windowId);

private:
    struct Entry final
    {
        qint64 processId = 0;
        QString resourceClass;
        Identity identity;
        quint64 generation = 0;
    };

    void startWorkerJob(const QString &windowId, const Entry &entry,
                        const QByteArray &commandLine);
    void acceptSteamName(const QString &windowId, quint64 generation,
                         const QString &steamName);

    // Shared with in-flight jobs: they post a result only while `target` is
    // set, under the mutex, so a job never posts to a destroyed resolver.
    struct Mailbox;
    std::shared_ptr<Mailbox> m_mailbox;
    std::shared_ptr<WineIdentityCache> m_cache;
    ProcessReader m_reader;
    Executor m_executor;
    QThread *m_thread = nullptr;
    QObject *m_worker = nullptr;
    QHash<QString, Entry> m_entries;
    quint64 m_nextGeneration = 1;
    qsizetype m_commandLineReads = 0;
};

} // namespace QindaQt::Compositor::KWinIntegration
