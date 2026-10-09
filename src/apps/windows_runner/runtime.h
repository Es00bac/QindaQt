// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/windows_runner/executable_file.h>
#include <QByteArray>
#include <QProcessEnvironment>
#include <QStringList>
#include <QVector>
#include <functional>
#include <memory>
#include <atomic>

namespace QindaQt::Apps::WindowsHandler {
struct PinnedFile final {
    int descriptor = -1;
    QString path;
    QByteArray digest;
    quint64 bytes = 0;
    quint64 device = 0;
    quint64 inode = 0;
    qint64 changedSeconds = 0;
    qint64 changedNanoseconds = 0;
    ~PinnedFile();
    [[nodiscard]] bool current() const;
};
struct InstalledWine final {
    QString id;
    QString label;
    std::shared_ptr<PinnedFile> preloader;
    std::shared_ptr<PinnedFile> loader;
    std::shared_ptr<PinnedFile> server;
    std::shared_ptr<PinnedFile> bubblewrap;
    [[nodiscard]] bool current() const;
};
struct ProgramInspection final {
    std::shared_ptr<WindowsRunner::WindowsExecutableFile> program;
    QVector<std::shared_ptr<InstalledWine>> runtimes;
    QString error;
    QString preferredRuntimeId;
    bool preferInstaller = false;
};

// Worker-thread only: opens the actual PE and exact root-owned installed
// Portage runtime payloads. No user Proton discovery or runtime downloading.
[[nodiscard]] ProgramInspection inspectProgram(const QString &path, const QString &dataRoot = {});
// Exposed to the focused private helper test; production always supplies UID0
// and exact VDB content digest. Caller owns a stable opened handle, not a path.
[[nodiscard]] std::shared_ptr<PinnedFile> pinFile(
    const QString &path, quint32 owner, const QByteArray &installedMd5 = {},
    quint64 maximumBytes = 64ULL * 1024 * 1024);

enum class ProgramKind { Portable, Installer };
struct ProfileLaunch final {
    QString profileRoot;
    QString registrationId;
    QString launchId;
    QString message;
    int processId = -1;
    bool submitted = false;
    bool outcomeUnknown = false;
};

using NativeSpawn = std::function<int(
    const QStringList &arguments, const QProcessEnvironment &environment,
    const QVector<int> &descriptors, int &processId)>;

// Worker-thread only. Holds a kernel profile lock across receipt validation,
// image snapshot and one dispatch. Uses user-file provenance and retains data
// on every failure. A native-user-session exposure is an explicit setup choice;
// this wrapper pins payloads and is not presented as a security sandbox.
[[nodiscard]] ProfileLaunch launchProgram(
    const ProgramInspection &inspection, const std::shared_ptr<InstalledWine> &runtime,
    ProgramKind kind, const QString &dataRoot,
    const QProcessEnvironment &sessionEnvironment,
    const QString &reuseProfileRoot = {}, const NativeSpawn &spawn = {},
    const std::shared_ptr<std::atomic_bool> &cancelled = {});
[[nodiscard]] int spawnPinnedWine(const QStringList &arguments,
    const QProcessEnvironment &environment, const QVector<int> &descriptors,
    int &processId);
} // namespace QindaQt::Apps::WindowsHandler
