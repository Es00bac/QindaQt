// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVersionNumber>

#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace QindaQt::Apps::WindowsHandler {
namespace {
bool safeRootFile(const QString &path)
{
    const QFileInfo file(path);
    return file.isFile() && !file.isSymLink() && file.ownerId() == 0
        && !(file.permissions() & (QFile::WriteGroup | QFile::WriteOther));
}

QByteArray installedDigest(const QString &packageDirectory, const QString &path)
{
    const QString contentsPath = packageDirectory + QStringLiteral("/CONTENTS");
    if (!safeRootFile(contentsPath)) { return {}; }
    QFile contents(contentsPath);
    if (!contents.open(QIODevice::ReadOnly) || contents.size() > 8 * 1024 * 1024) { return {}; }
    const QByteArray prefix = QByteArrayLiteral("obj ") + QFile::encodeName(path) + ' ';
    while (!contents.atEnd()) {
        const QByteArray line = contents.readLine(8192);
        if (line.startsWith(prefix)) {
            const QByteArray digest = line.mid(prefix.size()).split(' ').value(0);
            return digest.size() == 32 ? QByteArray::fromHex(digest) : QByteArray{};
        }
    }
    return {};
}

std::shared_ptr<PinnedFile> packagePin(const QString &packageDirectory, const QString &path)
{
    const QByteArray md5 = installedDigest(packageDirectory, path);
    return md5.size() == 16 ? pinFile(path, 0, md5) : nullptr;
}

std::shared_ptr<PinnedFile> bubblewrapPin()
{
    const QDir packages(QStringLiteral("/var/db/pkg/sys-apps"));
    for (const QString &entry : packages.entryList({QStringLiteral("bubblewrap-*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto pin = packagePin(packages.filePath(entry), QStringLiteral("/usr/bin/bwrap"));
        if (pin) { return pin; }
    }
    return {};
}
} // namespace

PinnedFile::~PinnedFile()
{
    if (descriptor >= 0) { ::close(descriptor); }
}

bool PinnedFile::current() const
{
    struct stat held{}; struct stat named{};
    const QByteArray encoded = QFile::encodeName(path);
    return descriptor >= 0 && ::fstat(descriptor, &held) == 0
        && ::lstat(encoded.constData(), &named) == 0 && S_ISREG(named.st_mode)
        && static_cast<quint64>(held.st_dev) == device && static_cast<quint64>(named.st_dev) == device
        && static_cast<quint64>(held.st_ino) == inode && static_cast<quint64>(named.st_ino) == inode
        && held.st_size >= 0 && named.st_size >= 0
        && static_cast<quint64>(held.st_size) == bytes && static_cast<quint64>(named.st_size) == bytes
        && held.st_ctim.tv_sec == changedSeconds && held.st_ctim.tv_nsec == changedNanoseconds
        && named.st_ctim.tv_sec == changedSeconds && named.st_ctim.tv_nsec == changedNanoseconds;
}

std::shared_ptr<PinnedFile> pinFile(const QString &path, quint32 owner,
    const QByteArray &installedMd5, quint64 maximumBytes)
{
    const QByteArray encoded = QFile::encodeName(path);
    const int fd = ::open(encoded.constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { return {}; }
    auto pin = std::make_shared<PinnedFile>(); pin->descriptor = fd; pin->path = path;
    struct stat attributes{};
    if (::fstat(fd, &attributes) != 0 || !S_ISREG(attributes.st_mode)
        || attributes.st_uid != owner || attributes.st_size <= 0
        || static_cast<quint64>(attributes.st_size) > maximumBytes
        || (attributes.st_mode & (S_IWGRP | S_IWOTH)) != 0) { return {}; }
    QCryptographicHash sha(QCryptographicHash::Sha256), md5(QCryptographicHash::Md5);
    QByteArray buffer(64 * 1024, '\0'); qint64 offset = 0;
    while (offset < attributes.st_size) {
        const ssize_t amount = ::pread(fd, buffer.data(), static_cast<size_t>(buffer.size()), offset);
        if (amount <= 0) { return {}; }
        sha.addData(QByteArrayView(buffer.constData(), amount));
        md5.addData(QByteArrayView(buffer.constData(), amount)); offset += amount;
    }
    if (!installedMd5.isEmpty() && md5.result() != installedMd5) { return {}; }
    pin->digest = sha.result(); pin->bytes = static_cast<quint64>(attributes.st_size);
    pin->device = static_cast<quint64>(attributes.st_dev); pin->inode = static_cast<quint64>(attributes.st_ino);
    pin->changedSeconds = static_cast<qint64>(attributes.st_ctim.tv_sec);
    pin->changedNanoseconds = static_cast<qint64>(attributes.st_ctim.tv_nsec);
    return pin->current() ? pin : nullptr;
}

bool InstalledWine::current() const
{
    return preloader && loader && server && bubblewrap
        && preloader->current() && loader->current() && server->current() && bubblewrap->current();
}

ProgramInspection inspectProgram(const QString &path, const QString &dataRoot)
{
    ProgramInspection result;
    auto inspected = WindowsRunner::inspectWindowsExecutable(path);
    if (!inspected.file) { result.error = inspected.message; return result; }
    if (inspected.file->architecture() == WindowsRunner::WindowsImageArchitecture::Arm64) {
        result.error = QStringLiteral("This program needs an ARM64 Windows runtime. The installed Wine adapter supports x86 and x64.");
        return result;
    }
    result.program = std::shared_ptr<WindowsRunner::WindowsExecutableFile>(std::move(inspected.file));
    if (!dataRoot.isEmpty()) {
        const QString key = QString::fromLatin1(QCryptographicHash::hash(
            result.program->canonicalPath().toUtf8(), QCryptographicHash::Sha256).toHex());
        const QString receipt = dataRoot + QLatin1Char('/') + key + QStringLiteral("/registration.json");
        if (QFileInfo::exists(receipt)) {
            QFile stored(receipt);
            if (QFileInfo(receipt).isSymLink() || !stored.open(QIODevice::ReadOnly) || stored.size() > 65536) {
                result.error = QStringLiteral("This Windows profile's settings need repair. Its data was retained."); return result;
            }
            const QJsonDocument document = QJsonDocument::fromJson(stored.readAll());
            if (!document.isObject() || document.object().value(QStringLiteral("schema")).toInt() != 1) {
                result.error = QStringLiteral("This Windows profile has unknown settings. Its data was retained."); return result;
            }
            result.preferredRuntimeId = document.object().value(QStringLiteral("runtimeCpv")).toString();
            result.preferInstaller = document.object().value(QStringLiteral("kind")).toString() == QStringLiteral("installer");
        }
    }
    const auto bwrap = bubblewrapPin();
    if (!bwrap) { result.error = QStringLiteral("Install the Portage bubblewrap package before running Windows programs."); return result; }
    const QDir packages(QStringLiteral("/var/db/pkg/app-emulation"));
    for (const QString &entry : packages.entryList({QStringLiteral("wine-proton-*")}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString package = packages.filePath(entry);
        const QFileInfo packageInfo(package);
        if (packageInfo.isSymLink() || packageInfo.ownerId() != 0
            || (packageInfo.permissions() & (QFile::WriteGroup | QFile::WriteOther))) { continue; }
        QString version = entry.mid(QStringLiteral("wine-proton-").size());
        version.remove(QRegularExpression(QStringLiteral("-r[0-9]+$")));
        if (!QRegularExpression(QStringLiteral("^[0-9][0-9.]+$")).match(version).hasMatch()) { continue; }
        const QString base = QStringLiteral("/usr/lib/wine-proton-") + version;
        auto runtime = std::make_shared<InstalledWine>();
        runtime->id = QStringLiteral("app-emulation/") + entry;
        runtime->label = QStringLiteral("Wine Proton %1").arg(version);
        runtime->preloader = packagePin(package, base + QStringLiteral("/wine/x86_64-unix/wine-preloader"));
        runtime->loader = packagePin(package, base + QStringLiteral("/wine/x86_64-unix/wine"));
        runtime->server = packagePin(package, base + QStringLiteral("/bin/wineserver"));
        runtime->bubblewrap = bwrap;
        if (runtime->current()) { result.runtimes.append(runtime); }
    }
    if (result.runtimes.isEmpty()) {
        result.error = QStringLiteral("No verified Portage Wine runtime is available. Install app-emulation/wine-proton, then try again.");
    }
    return result;
}

int spawnPinnedWine(const QStringList &arguments, const QProcessEnvironment &environment,
    const QVector<int> &descriptors, int &processId)
{
    // Slots3..8 are image, preloader, loader, server, bwrap and profile lock.
    // Duplicate all sources high first: sequential dup2 must not overwrite a
    // later source. closefrom removes ambient GUI/bus/secret descriptors.
    if (descriptors.size() != 6) { return EINVAL; }
    QVector<int> copies;
    for (const int fd : descriptors) {
        const int copy = ::fcntl(fd, F_DUPFD_CLOEXEC, 64);
        if (copy < 0) { for (const int held : copies) { ::close(held); } return errno; }
        copies.append(copy);
    }
    posix_spawn_file_actions_t actions;
    int error = ::posix_spawn_file_actions_init(&actions);
    if (error) { for (const int held : copies) { ::close(held); } return error; }
    for (qsizetype i = 0; i < copies.size() && !error; ++i) {
        error = ::posix_spawn_file_actions_adddup2(&actions, copies[i], static_cast<int>(i) + 3);
    }
    if (!error) { error = ::posix_spawn_file_actions_addclosefrom_np(&actions, 9); }
    std::vector<QByteArray> argBytes, envBytes;
    argBytes.push_back(QByteArrayLiteral("bwrap"));
    for (const QString &argument : arguments) { argBytes.push_back(argument.toUtf8()); }
    for (const QString &assignment : environment.toStringList()) { envBytes.push_back(assignment.toUtf8()); }
    std::vector<char *> argv, env;
    for (auto &bytes : argBytes) { argv.push_back(bytes.data()); } argv.push_back(nullptr);
    for (auto &bytes : envBytes) { env.push_back(bytes.data()); } env.push_back(nullptr);
    pid_t pid = -1;
    if (!error) { error = ::posix_spawn(&pid, "/proc/self/fd/7", &actions, nullptr, argv.data(), env.data()); }
    ::posix_spawn_file_actions_destroy(&actions);
    for (const int held : copies) { ::close(held); }
    if (!error) { processId = static_cast<int>(pid); }
    return error;
}
} // namespace QindaQt::Apps::WindowsHandler
