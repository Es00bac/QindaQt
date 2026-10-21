// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QString>
#include <optional>
#include <unordered_map>

namespace QindaQt::Controllers {
// Decides, for one proc scan, which processes holding a controller node count
// as "a game" for the application that owns the focused window (ADR-0428).
//
// Two questions, both answered from /proc facts that are read lazily and cached
// for the life of this object, so a process nobody asks about is never read:
//  - passiveWebEngineHost(): the process embeds Chromium, Gecko, CEF or
//    QtWebEngine, whose gamepad service opens every pad for the life of the
//    process once any page touches the Gamepad API. That open says nothing
//    about what the user is doing, so it never pauses desktop control.
//  - partOfFocusedApplication(): the process is the focused window's process or
//    belongs to the same application: ancestry, a private session, a private
//    cgroup, or one Wine prefix.
//
// Threading: observation worker only (ADR-0397). Not thread-safe; construct one
// per scan. No SDL, KWin or input-sink access. Error behavior: a fact that
// cannot be read (vanished process, no ptrace-read access) is unknown; an
// unknown fact never establishes a relation, and an unclassifiable holder is not
// passive, so failures keep the pre-ADR-0428 outcome of treating it as a game.
class FocusOwnership final {
public:
    // The compositor's own identity. Services spawned by the session share its
    // session id and cgroup, so those values must never relate two processes.
    // A non-positive session or empty cgroup disables the rule that needs it.
    struct Compositor {
        qint64 pid = 0;
        qint64 session = 0;
        QString cgroup;
    };
    FocusOwnership(QString procRoot, qint64 focusedPid, Compositor compositor);
    // Reads this process's mappings once, bounded. A holder (the only caller)
    // is already known to be readable because its FD directory was.
    bool passiveWebEngineHost(qint64 pid);
    // `pid` must be a process that holds a watched node (its mappings may be
    // read). False when no application is focused or the compositor owns the focus.
    bool partOfFocusedApplication(qint64 pid);
    // /proc/<pid>/comm reduced to a short, printable name for diagnostics.
    QString displayName(qint64 pid);
private:
    struct Process {
        QString directory;
        std::optional<bool> sameUser;
        bool identityRead = false, cgroupRead = false, exeRead = false, mapsRead = false, commRead = false, prefixRead = false;
        qint64 parent = 0, session = 0;
        QString cgroup, exeName;
        QByteArray comm;
        bool passive = false, wineLoader = false;
        std::optional<QString> winePrefix;
    };
    Process &process(qint64 pid);
    qint64 parentOf(qint64 pid);
    qint64 sessionOf(qint64 pid);
    const QString &cgroupOf(qint64 pid);
    bool sameUser(qint64 pid);
    bool ancestorOf(qint64 ancestor, qint64 descendant);
    bool shareSession(qint64 first, qint64 second);
    bool shareCgroup(qint64 first, qint64 second);
    bool shareWinePrefix(qint64 holder, qint64 focused);
    bool isWine(qint64 pid);
    const std::optional<QString> &winePrefixOf(qint64 pid);
    void readIdentity(Process &p);
    void scanMaps(Process &p);
    const QByteArray &commOf(qint64 pid);
    QString m_root;
    qint64 m_focused;
    Compositor m_compositor;
    // Node-based on purpose: helpers hold a Process& across calls that add
    // other processes, so references must survive insertion.
    std::unordered_map<qint64, Process> m_processes;
};
} // namespace QindaQt::Controllers
