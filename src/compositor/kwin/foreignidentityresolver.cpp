// SPDX-License-Identifier: GPL-3.0-or-later
#include "foreignidentityresolver.h"

#include "qindaqt/compositor/foreignwindowidentity.h"
#include "qindaqt/compositor/wineidentitycache.h"
#include "qindaqt/compositor/winelaunchpaths.h"

#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

// AGENT-GUARD: `pid` must be KWin's authenticated client PID (wl_client
// credentials, or XRes LOCAL_CLIENT_PID on X11), never a client-supplied
// `_NET_WM_PID`, which would let one client borrow another program's
// identity. Reads are capped and failure is silent: an exited or unreadable
// process simply yields no better identity (ADR-0169).
QByteArray readProcessFile(qint64 pid, const char *name, qint64 limit)
{
    if (pid <= 1) {
        return {};
    }
    QFile file(QStringLiteral("/proc/%1/%2").arg(pid).arg(QString::fromLatin1(name)));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.read(limit);
}

ForeignIdentityResolver::ProcessReader withDefaults(ForeignIdentityResolver::ProcessReader reader)
{
    if (!reader.commandLine) {
        reader.commandLine = [](qint64 pid) { return readProcessFile(pid, "cmdline", 8192); };
    }
    if (!reader.environment) {
        reader.environment = [](qint64 pid) { return readProcessFile(pid, "environ", 65536); };
    }
    return reader;
}

} // namespace

struct ForeignIdentityResolver::Mailbox final
{
    QMutex mutex;
    ForeignIdentityResolver *target = nullptr;
};

ForeignIdentityResolver::ForeignIdentityResolver(std::shared_ptr<WineIdentityCache> cache,
                                                 ProcessReader reader, Executor executor,
                                                 QObject *parent)
    : QObject(parent)
    , m_mailbox(std::make_shared<Mailbox>())
    , m_cache(std::move(cache))
    , m_reader(withDefaults(std::move(reader)))
    , m_executor(std::move(executor))
{
    m_mailbox->target = this;
    if (!m_executor) {
        m_thread = new QThread;
        m_thread->setObjectName(QStringLiteral("qindaqt-foreign-identity"));
        m_worker = new QObject;
        m_worker->moveToThread(m_thread);
        // The worker is deleted on its own thread as that thread finishes.
        connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
        m_thread->start(QThread::LowPriority);
        QObject *const worker = m_worker;
        m_executor = [worker](Job job) {
            QMetaObject::invokeMethod(worker, std::move(job), Qt::QueuedConnection);
        };
    }
}

ForeignIdentityResolver::~ForeignIdentityResolver()
{
    {
        QMutexLocker lock(&m_mailbox->mutex);
        m_mailbox->target = nullptr;
    }
    if (!m_thread) {
        return;
    }
    m_thread->quit();
    // AGENT-GUARD: a job may sit in a slow filesystem read (a Wine prefix on
    // a network mount). Wait a bounded time; if it is still running, leak the
    // thread and worker rather than destroy a running QThread, which aborts.
    // The cleared mailbox above stops it from posting to this object.
    if (m_thread->wait(2000)) {
        delete m_thread;
    } else {
        qWarning("QindaQt foreign window identity worker did not stop; leaving it to exit");
    }
}

ForeignIdentityResolver::Identity ForeignIdentityResolver::identity(
    const QString &windowId, qint64 processId, const QString &resourceClass)
{
    const auto found = m_entries.constFind(windowId);
    if (found != m_entries.cend() && found->processId == processId
        && found->resourceClass == resourceClass) {
        return found->identity;
    }
    if (found == m_entries.cend() && m_entries.size() >= MaximumTrackedWindows) {
        // Windows are forgotten on close; this only bounds a pathological
        // session. Dropping everything merely costs one re-read per window.
        m_entries.clear();
    }
    Entry entry{processId, resourceClass, {}, m_nextGeneration++};
    const QByteArray commandLine = m_reader.commandLine(processId);
    ++m_commandLineReads;
    entry.identity.clientExecutable = Compositor::executableFromCommandLine(commandLine);
    m_entries.insert(windowId, entry);
    startWorkerJob(windowId, entry, commandLine);
    return entry.identity;
}

void ForeignIdentityResolver::forget(const QString &windowId)
{
    m_entries.remove(windowId);
}

void ForeignIdentityResolver::startWorkerJob(const QString &windowId, const Entry &entry,
                                             const QByteArray &commandLine)
{
    if (!m_cache) {
        return;
    }
    auto mailbox = m_mailbox;
    auto cache = m_cache;
    auto environment = m_reader.environment;
    const qint64 processId = entry.processId;
    const QString resourceClass = entry.resourceClass;
    const quint64 generation = entry.generation;
    m_executor([mailbox, cache, environment, processId, resourceClass, generation,
                windowId, commandLine] {
        // Worker side: everything up to the post may block on the filesystem.
        const QString steamName = cache->steamNameForClass(resourceClass);
        if (!Compositor::windowsExecutablePathFromCommandLine(commandLine).isEmpty()) {
            cache->ensureIconForClient(commandLine, environment(processId));
        }
        // AGENT-GUARD: post only while holding the mailbox lock with a live
        // target; the resolver clears it under the same lock before it is
        // destroyed, and Qt drops events still queued for a destroyed object.
        QMutexLocker lock(&mailbox->mutex);
        if (auto *const target = mailbox->target) {
            QMetaObject::invokeMethod(
                target,
                [target, windowId, generation, steamName] {
                    target->acceptSteamName(windowId, generation, steamName);
                },
                Qt::QueuedConnection);
        }
    });
}

void ForeignIdentityResolver::acceptSteamName(const QString &windowId, quint64 generation,
                                              const QString &steamName)
{
    auto found = m_entries.find(windowId);
    if (found == m_entries.end() || found->generation != generation
        || found->identity.steamName == steamName) {
        return;
    }
    found->identity.steamName = steamName;
    Q_EMIT identityChanged(windowId);
}

} // namespace QindaQt::Compositor::KWinIntegration
