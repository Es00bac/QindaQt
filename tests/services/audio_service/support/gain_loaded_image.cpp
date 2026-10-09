// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "gain_loaded_image.h"
#include <pipewire/impl-module.h>
#include <pipewire/pipewire.h>
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <dlfcn.h>
#include <link.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <limits>

namespace QindaQt::Audio::Test
{
namespace {
constexpr qint64 kMaxImageBytes = 128 * 1024 * 1024;
constexpr qint64 kMaxMapsBytes = 2 * 1024 * 1024;
bool sameVersion(const struct stat &a, const struct stat &b)
{
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size
        && a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec
        && a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
bool fileStat(const GainImageArtifact &artifact, struct stat &value)
{
    if (artifact.bytes <= 0 || artifact.bytes > kMaxImageBytes
        || artifact.sha256.size() != 64 || artifact.path.size() > 4096
        || QFileInfo(artifact.path).canonicalFilePath() != artifact.path)
        return false;
    return ::lstat(QFile::encodeName(artifact.path).constData(), &value) == 0
        && S_ISREG(value.st_mode) && value.st_size == artifact.bytes
        && !(value.st_mode & (S_IWGRP | S_IWOTH));
}
bool mapsMatch(const QString &mapsPath, const struct stat &file, quintptr address = 0)
{
    QFile maps(mapsPath);
    if (!maps.open(QIODevice::ReadOnly)) return false;
    const QByteArray content = maps.read(kMaxMapsBytes + 1);
    if (content.isEmpty() || content.size() > kMaxMapsBytes) return false;
    for (const QByteArray &line : content.split('\n')) {
        const auto fields = line.simplified().split(' ');
        if (fields.size() < 5) continue;
        const auto range = fields.at(0).split('-');
        const auto device = fields.at(3).split(':');
        if (range.size() != 2 || device.size() != 2) continue;
        bool a = false, b = false, c = false, d = false, e = false;
        const auto start = range.at(0).toULongLong(&a, 16);
        const auto end = range.at(1).toULongLong(&b, 16);
        const auto majorNumber = device.at(0).toULongLong(&c, 16);
        const auto minorNumber = device.at(1).toULongLong(&d, 16);
        const auto inode = fields.at(4).toULongLong(&e);
        if (a && b && c && d && e && start < end
            && inode == static_cast<quint64>(file.st_ino)
            && majorNumber == static_cast<quint64>(major(file.st_dev))
            && minorNumber == static_cast<quint64>(minor(file.st_dev))
            && (address == 0 || (address >= start && address < end)))
            return true;
    }
    return false;
}
struct ModuleMapping {
    const GainImageArtifact *artifact = nullptr;
    struct stat file{};
    int matchingImages = 0;
    bool mapped = false;
};
int moduleMapping(dl_phdr_info *info, size_t, void *data)
{
    auto *result = static_cast<ModuleMapping *>(data);
    if (!info->dlpi_name || strnlen(info->dlpi_name, 4097) > 4096
        || QFileInfo(QString::fromUtf8(info->dlpi_name)).canonicalFilePath()
            != result->artifact->path)
        return 0;
    ++result->matchingImages;
    for (quint16 i = 0; i < info->dlpi_phnum; ++i) {
        const auto &segment = info->dlpi_phdr[i];
        if (segment.p_type == PT_LOAD && segment.p_filesz > 0) {
            const auto address = static_cast<quintptr>(info->dlpi_addr + segment.p_vaddr);
            result->mapped = mapsMatch(QStringLiteral("/proc/self/maps"), result->file, address);
            break;
        }
    }
    return 0;
}
} // namespace

bool exactGainImageFile(const GainImageArtifact &artifact)
{
    struct stat before{}, after{}, named{};
    if (!fileStat(artifact, named)) return false;
    const int fd = ::open(QFile::encodeName(artifact.path).constData(),
                          O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return false;
    bool accepted = ::fstat(fd, &before) == 0 && S_ISREG(before.st_mode)
        && sameVersion(before, named);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buffer[64 * 1024];
    qint64 total = 0;
    while (accepted) {
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { accepted = false; break; }
        if (count == 0) break;
        total += count;
        if (total > artifact.bytes) { accepted = false; break; }
        hash.addData(QByteArrayView(buffer, count));
    }
    accepted = accepted && total == artifact.bytes
        && hash.result().toHex() == artifact.sha256
        && ::fstat(fd, &after) == 0 && sameVersion(before, after)
        && fileStat(artifact, named) && sameVersion(after, named);
    ::close(fd);
    return accepted;
}
bool actualGainCoreImage(const GainImageArtifact &artifact)
{
    Dl_info info{};
    struct stat file{};
    // Resolve the actual function, not a non-PIC executable's possible PLT.
    void *address = dlsym(RTLD_DEFAULT, "pw_context_load_module");
    return exactGainImageFile(artifact) && fileStat(artifact, file)
        && address && dladdr(address, &info) != 0 && info.dli_fname
        && strnlen(info.dli_fname, 4097) <= 4096
        && QFileInfo(QString::fromUtf8(info.dli_fname)).canonicalFilePath() == artifact.path
        && mapsMatch(QStringLiteral("/proc/self/maps"), file,
                     reinterpret_cast<quintptr>(address));
}
bool actualGainModuleImage(pw_impl_module *module, const GainImageArtifact &artifact)
{
    if (!module || !exactGainImageFile(artifact)) return false;
    const auto *info = pw_impl_module_get_info(module);
    if (!info || !info->filename || strnlen(info->filename, 4097) > 4096
        || QFileInfo(QString::fromUtf8(info->filename)).canonicalFilePath() != artifact.path)
        return false;
    ModuleMapping mapping{.artifact = &artifact};
    if (!fileStat(artifact, mapping.file)) return false;
    dl_iterate_phdr(moduleMapping, &mapping);
    return mapping.matchingImages == 1 && mapping.mapped;
}
bool gainProcessMapsImage(const qint64 privateChildPid, const GainImageArtifact &artifact)
{
    if (privateChildPid <= 0) return false;
    struct stat file{};
    return exactGainImageFile(artifact) && fileStat(artifact, file)
        && mapsMatch(QStringLiteral("/proc/%1/maps").arg(privateChildPid), file);
}
} // namespace QindaQt::Audio::Test
