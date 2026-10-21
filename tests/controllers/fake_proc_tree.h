// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/controllers/game_priority.h"
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <optional>

namespace QindaQt::Controllers::Testing {
// One fake same-user process. Defaults describe an unremarkable program that
// shares nothing with any other fake process: its own session, its own cgroup,
// no web engine and no Wine, so a test names only the facts it relies on.
struct FakeProcess {
    QString comm = QStringLiteral("game");
    qint64 parent = 1;
    qint64 session = 0;                      // 0: the process's own pid
    QString cgroup = {};                     // empty: a private cgroup named after the pid
    QByteArray maps = "7f0000000000-7f0000001000 r-xp 00000000 00:00 0 /usr/lib64/libc.so.6\n";
    std::optional<QByteArray> environment = std::nullopt;   // unset: the file is absent (unreadable)
    QString exe = {};                        // empty: no exe link
    QStringList holds = {};                  // nodes this process has open
};
// A throw-away /proc, /dev and /sys that the production inspector reads exactly
// like the real ones. Header-only so the inspection and runtime fixtures share
// one description of a process; nothing here touches a live process.
class FakeProcTree {
public:
    static constexpr qint64 CompositorPid = 99;
    static constexpr qint64 CompositorSession = 7000;
    static inline const QString CompositorCgroup =
        QStringLiteral("/user.slice/user-1000.slice/user@1000.service/session.slice/qindaqt-compositor.service");
    FakeProcTree() { QDir().mkpath(root()); }
    bool isValid() const { return m_dir.isValid(); }
    QString root() const { return m_dir.filePath("proc"); }
    QString devRoot() const { return m_dir.filePath("dev"); }
    QString sysRoot() const { return m_dir.filePath("sys"); }
    // An empty input node; the returned absolute path is what FD links name.
    QString device(const QString &name) const {
        const auto path = devRoot() + "/input/" + name;
        QDir().mkpath(devRoot() + "/input");
        write(path, {});
        return path;
    }
    // Makes controllerDevicePaths() match `node` to a controller with this
    // vendor and product, as /sys/class/input/<node>/device/id does.
    void describeInputNode(const QString &name, quint16 vendor, quint16 product) const {
        const auto id = sysRoot() + "/class/input/" + name + "/device/id";
        QDir().mkpath(id);
        write(id + "/vendor", QByteArray::number(vendor, 16).rightJustified(4, '0') + '\n');
        write(id + "/product", QByteArray::number(product, 16).rightJustified(4, '0') + '\n');
    }
    void add(qint64 pid, const FakeProcess &process) const {
        const auto dir = root() + '/' + QString::number(pid);
        QDir().mkpath(dir + "/fd");
        write(dir + "/comm", process.comm.toUtf8() + '\n');
        // Field 2 is the command in parentheses, then: state ppid pgrp session.
        write(dir + "/stat", QByteArray::number(pid) + " (" + process.comm.toUtf8() + ") S "
            + QByteArray::number(process.parent) + ' ' + QByteArray::number(pid) + ' '
            + QByteArray::number(process.session ? process.session : pid) + " 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 1 0 0\n");
        write(dir + "/cgroup", "0::" + (process.cgroup.isEmpty() ? QStringLiteral("/app.slice/private-%1.scope").arg(pid) : process.cgroup).toUtf8() + '\n');
        write(dir + "/maps", process.maps);
        if (process.environment) write(dir + "/environ", *process.environment);
        if (!process.exe.isEmpty()) QFile::link(process.exe, dir + "/exe");
        int descriptor = 3;
        for (const auto &node : process.holds) QFile::link(node, dir + "/fd/" + QString::number(descriptor++));
    }
    void remove(qint64 pid) const { QDir(root() + '/' + QString::number(pid)).removeRecursively(); }
    // A scan of `paths` as the compositor (pid CompositorPid) would run it.
    GamePriorityQuery query(QSet<QString> paths, qint64 focusedPid) const {
        GamePriorityQuery result;
        result.paths = std::move(paths);
        result.focusedPid = focusedPid;
        result.selfPid = CompositorPid;
        result.compositorSession = CompositorSession;
        result.compositorCgroup = CompositorCgroup;
        return result;
    }
private:
    static void write(const QString &path, const QByteArray &data) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) file.write(data);
    }
    QTemporaryDir m_dir;
};
} // namespace QindaQt::Controllers::Testing
