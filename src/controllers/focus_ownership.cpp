// SPDX-License-Identifier: GPL-3.0-or-later
#include "focus_ownership_p.h"
#include "proc_files_p.h"
#include "qindaqt/controllers/controller_policy.h"
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <unistd.h>

namespace QindaQt::Controllers {
namespace {
constexpr int MaxAncestorDepth = 32;
// One pass over /proc/<pid>/maps answers both questions below. Bits 0-3 mean
// "hosts a web engine": Qt WebEngine and Gecko are named by their core library;
// Chromium, Electron, NW.js and CEF all map their ICU data file, which Chromium
// itself needs to start. Bits 4-7 mean "Wine loader image"; the trailing
// newline anchors the match to the end of a mapping's path.
constexpr quint32 WebEngineMask = 0x0F, WineLoaderMask = 0xF0;
const QList<QByteArray> &mapNeedles() {
    static const QList<QByteArray> needles{
        "/libQt6WebEngineCore.so", "/libxul.so", "/libcef.so", "icudtl.dat",
        "/wine\n", "/wine64\n", "/wine-preloader\n", "/wine64-preloader\n"};
    return needles;
}
bool wineLoaderName(const QString &name) {
    return name == "wine" || name == "wine64" || name == "wine-preloader" || name == "wine64-preloader";
}
// Login sessions (session-2.scope) hold every app the user started from one
// login, so sharing one says nothing about two processes being one program.
bool loginSessionScope(const QString &cgroup) {
    const auto leaf = cgroup.mid(cgroup.lastIndexOf('/') + 1);
    return leaf.startsWith(QLatin1String("session-")) && leaf.endsWith(QLatin1String(".scope"));
}
}
FocusOwnership::FocusOwnership(QString procRoot, qint64 focusedPid, Compositor compositor)
    : m_root(std::move(procRoot)), m_focused(focusedPid), m_compositor(std::move(compositor)) {}
FocusOwnership::Process &FocusOwnership::process(qint64 pid) {
    auto it = m_processes.find(pid);
    if (it == m_processes.end()) {
        it = m_processes.emplace(pid, Process{}).first;
        it->second.directory = m_root + '/' + QString::number(pid);
    }
    return it->second;
}
void FocusOwnership::readIdentity(Process &p) {
    p.identityRead = true;
    // Fields after the last ')' are: state ppid pgrp session ... The command
    // name before it may itself contain spaces and parentheses.
    const auto stat = ProcFiles::read(p.directory + "/stat", ProcFiles::SmallLimit);
    const auto close = stat.lastIndexOf(')');
    if (close < 0) return;
    const auto fields = stat.mid(close + 1).trimmed().split(' ');
    if (fields.size() < 4) return;
    bool parentValid = false, sessionValid = false;
    const auto parent = fields[1].toLongLong(&parentValid);
    const auto session = fields[3].toLongLong(&sessionValid);
    if (parentValid) p.parent = parent;
    if (sessionValid) p.session = session;
}
qint64 FocusOwnership::parentOf(qint64 pid) {
    auto &p = process(pid);
    if (!p.identityRead) readIdentity(p);
    return p.parent;
}
qint64 FocusOwnership::sessionOf(qint64 pid) {
    auto &p = process(pid);
    if (!p.identityRead) readIdentity(p);
    return p.session;
}
const QString &FocusOwnership::cgroupOf(qint64 pid) {
    auto &p = process(pid);
    if (!p.cgroupRead) {
        p.cgroupRead = true;
        p.cgroup = ProcFiles::cgroupPath(p.directory);
    }
    return p.cgroup;
}
const QByteArray &FocusOwnership::commOf(qint64 pid) {
    auto &p = process(pid);
    if (!p.commRead) {
        p.commRead = true;
        p.comm = ProcFiles::read(p.directory + "/comm", 64).trimmed();
    }
    return p.comm;
}
bool FocusOwnership::sameUser(qint64 pid) {
    auto &p = process(pid);
    // A same-user process that is not dumpable shows a root-owned directory and
    // cannot be inspected either; it ends an ancestry walk like a foreign user.
    if (!p.sameUser) p.sameUser = QFileInfo(p.directory).ownerId() == ::getuid();
    return *p.sameUser;
}
void FocusOwnership::scanMaps(Process &p) {
    p.mapsRead = true;
    const auto found = ProcFiles::findAny(p.directory + "/maps", mapNeedles(), WebEngineMask, ProcFiles::MapsLimit);
    p.passive = (found & WebEngineMask) != 0;
    p.wineLoader = (found & WineLoaderMask) != 0;
}
bool FocusOwnership::passiveWebEngineHost(qint64 pid) {
    auto &p = process(pid);
    if (!p.mapsRead) scanMaps(p);
    return p.passive;
}
bool FocusOwnership::ancestorOf(qint64 ancestor, qint64 descendant) {
    qint64 current = descendant;
    for (int depth = 0; depth < MaxAncestorDepth; ++depth) {
        const auto parent = parentOf(current);
        if (parent <= 0) return false;
        if (parent == ancestor) return true;
        // AGENT-GUARD: pid 1, the compositor and other users' processes are
        // ancestors of unrelated applications. Walking through them would make
        // every app one "application" with the compositor or the session manager.
        if (parent == 1 || parent == m_compositor.pid || !sameUser(parent)) return false;
        current = parent;
    }
    return false;
}
bool FocusOwnership::shareSession(qint64 first, qint64 second) {
    const auto session = sessionOf(first);
    // AGENT-GUARD: the compositor's own session also holds every service that
    // qindaqt-session starts, so it can never mean "the same application".
    return m_compositor.session > 0 && session > 0 && session == sessionOf(second) && session != m_compositor.session;
}
bool FocusOwnership::shareCgroup(qint64 first, qint64 second) {
    const auto &cgroup = cgroupOf(first);
    // AGENT-GUARD: login-session scopes, the compositor's own cgroup and the
    // root cgroup contain unrelated applications; only a private unit (for
    // example a systemd-run --user --scope game) identifies one program.
    return !m_compositor.cgroup.isEmpty() && !cgroup.isEmpty() && cgroup != "/" && cgroup == cgroupOf(second)
        && cgroup != m_compositor.cgroup && !loginSessionScope(cgroup);
}
bool FocusOwnership::isWine(qint64 pid) {
    auto &p = process(pid);
    // Wine names a process after its Windows image, cut to the kernel's 15
    // characters, so "Game.exe" may read "Game.e"; the loader image identifies
    // those. Mappings count only for a holder whose mappings were scanned.
    const auto comm = QString::fromUtf8(commOf(pid)).toLower();
    if (comm.endsWith(QLatin1String(".exe")) || wineLoaderName(comm)) return true;
    if (!p.exeRead) {
        p.exeRead = true;
        auto target = ProcFiles::linkTarget(p.directory + "/exe");
        if (target.endsWith(QLatin1String(" (deleted)"))) target.chop(10);
        p.exeName = target.mid(target.lastIndexOf('/') + 1);
    }
    return wineLoaderName(p.exeName) || (p.mapsRead && p.wineLoader);
}
const std::optional<QString> &FocusOwnership::winePrefixOf(qint64 pid) {
    auto &p = process(pid);
    if (!p.prefixRead) {
        p.prefixRead = true;
        const auto environment = ProcFiles::read(p.directory + "/environ", ProcFiles::EnvironLimit);
        // A truncated or unreadable environment cannot prove the variable is unset.
        if (!environment.isEmpty() && environment.size() < ProcFiles::EnvironLimit) {
            QString prefix;
            for (const auto &entry : environment.split('\0'))
                if (entry.startsWith("WINEPREFIX=")) { prefix = QString::fromUtf8(entry.mid(11)); break; }
            p.winePrefix = QDir::cleanPath(prefix.isEmpty() ? QDir::homePath() + "/.wine" : prefix);
        }
    }
    return p.winePrefix;
}
bool FocusOwnership::shareWinePrefix(qint64 holder, qint64 focused) {
    // The holder's mappings may name a Wine loader; read them here rather than
    // relying on an earlier passiveWebEngineHost() call. The focused process need
    // not hold anything, so only its comm and image are consulted.
    auto &mappings = process(holder);
    if (!mappings.mapsRead) scanMaps(mappings);
    if (!isWine(holder) || !isWine(focused)) return false;
    // winedevice.exe (which holds the HID nodes through winebus) and the game's
    // own .exe window run under the same prefix but in different sessions.
    const auto &prefix = winePrefixOf(holder);
    return prefix && prefix == winePrefixOf(focused);
}
bool FocusOwnership::partOfFocusedApplication(qint64 pid) {
    // The compositor's own windows are not an application that can own a game.
    if (m_focused <= 0 || m_focused == m_compositor.pid) return false;
    return pid == m_focused
        || ancestorOf(pid, m_focused) || ancestorOf(m_focused, pid)
        || shareSession(pid, m_focused)
        || shareCgroup(pid, m_focused)
        || shareWinePrefix(pid, m_focused);
}
QString FocusOwnership::displayName(qint64 pid) {
    // The kernel truncates comm to 15 bytes, which can split a UTF-8 sequence.
    return displayableConsumerName(QString::fromUtf8(commOf(pid)));
}
} // namespace QindaQt::Controllers
