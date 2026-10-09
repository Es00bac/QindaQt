// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "wireplumber_send_image_p.h"
#include "wireplumber_send_policy.h"

#include <pipewire/impl-module.h>
#include <pipewire/pipewire.h>
#include <wp/wp.h>
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

namespace QindaQt::Audio
{
namespace {
bool sameFile(const struct stat &a, const struct stat &b)
{
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size
        && a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec
        && a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}

bool pinnedFile(const QString &path, const QByteArray &digest, struct stat *identity = nullptr)
{
    struct stat named{}, before{}, after{};
    if (digest.size() != 64 || path.isEmpty() || path.size() > 4096
        || QFileInfo(path).canonicalFilePath() != path
        || ::lstat(QFile::encodeName(path).constData(), &named) != 0
        || !S_ISREG(named.st_mode) || named.st_uid != 0
        || (named.st_mode & (S_IWGRP | S_IWOTH)) || named.st_size <= 0
        || named.st_size > 128 * 1024 * 1024) return false;
    const int descriptor = ::open(QFile::encodeName(path).constData(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) return false;
    bool accepted = ::fstat(descriptor, &before) == 0 && sameFile(before, named);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buffer[65536];
    qint64 total = 0;
    while (accepted) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { accepted = false; break; }
        if (count == 0) break;
        total += count;
        if (total > before.st_size) { accepted = false; break; }
        hash.addData(QByteArrayView(buffer, count));
    }
    accepted = accepted && total == before.st_size && hash.result().toHex() == digest
        && ::fstat(descriptor, &after) == 0 && sameFile(before, after)
        && ::lstat(QFile::encodeName(path).constData(), &named) == 0 && sameFile(after, named);
    ::close(descriptor);
    if (accepted && identity) *identity = after;
    return accepted;
}

bool mappedFile(const struct stat &file, const quintptr address)
{
    QFile maps(QStringLiteral("/proc/self/maps"));
    if (!maps.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = maps.read(2 * 1024 * 1024 + 1);
    if (bytes.size() > 2 * 1024 * 1024) return false;
    for (const auto &line : bytes.split('\n')) {
        const auto fields = line.simplified().split(' ');
        if (fields.size() < 5) continue;
        const auto range = fields.at(0).split('-');
        const auto device = fields.at(3).split(':');
        if (range.size() != 2 || device.size() != 2) continue;
        bool a=false, b=false, c=false, d=false, e=false;
        const auto start=range.at(0).toULongLong(&a,16), end=range.at(1).toULongLong(&b,16);
        const auto maj=device.at(0).toULongLong(&c,16), min=device.at(1).toULongLong(&d,16);
        const auto inode=fields.at(4).toULongLong(&e);
        if (a && b && c && d && e && start < end && address >= start && address < end
            && inode == static_cast<quint64>(file.st_ino)
            && maj == static_cast<quint64>(major(file.st_dev))
            && min == static_cast<quint64>(minor(file.st_dev))) return true;
    }
    return false;
}

bool packageOwned()
{
    QFile repository(QStringLiteral("/var/db/pkg/media-video/pipewire-1.6.8-r1/repository"));
    return QByteArray(QINDAQT_AUDIO_SEND_PIPEWIRE_ATOM)
            == "=media-video/pipewire-1.6.8-r1::qindaqt"
        && repository.open(QIODevice::ReadOnly)
        && repository.read(32).trimmed() == QByteArray("qindaqt");
}

struct ModuleMapping {
    QString path;
    struct stat identity{};
    int matching = 0;
    bool mapped = false;
};
int observeModule(dl_phdr_info *info, size_t, void *data)
{
    auto *result = static_cast<ModuleMapping *>(data);
    if (!info->dlpi_name || strnlen(info->dlpi_name, 4097) > 4096
        || QFileInfo(QString::fromUtf8(info->dlpi_name)).canonicalFilePath() != result->path) return 0;
    ++result->matching;
    for (quint16 index = 0; index < info->dlpi_phnum; ++index) {
        const auto &segment = info->dlpi_phdr[index];
        if (segment.p_type == PT_LOAD && segment.p_filesz > 0) {
            result->mapped = mappedFile(result->identity,
                static_cast<quintptr>(info->dlpi_addr + segment.p_vaddr));
            break;
        }
    }
    return 0;
}
}

bool ConsoleSendImage::enabled() noexcept
{
    return QINDAQT_AUDIO_ADMITTED_SENDS != 0;
}

bool ConsoleSendImage::admitted() const
{
    if (!enabled() || !packageOwned() || QByteArray(pw_get_library_version()) != "1.6.8"
        || QByteArray(wp_get_library_version()) != "0.5.15") return false;
    const QString core(QStringLiteral(QINDAQT_AUDIO_SEND_CORE_FILE));
    const QString module(QStringLiteral(QINDAQT_AUDIO_SEND_MODULE_FILE));
    // AGENT-GUARD: the release service sets the exact module directory before
    // process entry. Never inherit a caller path or change it during factory
    // dispatch; the compile-pinned Core and module bytes must also be current.
    if (qgetenv("PIPEWIRE_MODULE_DIR") != QFileInfo(module).absolutePath().toUtf8()) return false;
    struct stat image{};
    Dl_info info{};
    void *symbol = dlsym(RTLD_DEFAULT, "pw_context_load_module");
    return pinnedFile(core, QByteArray(QINDAQT_AUDIO_SEND_CORE_SHA256), &image)
        && pinnedFile(module, QByteArray(QINDAQT_AUDIO_SEND_MODULE_SHA256))
        && symbol && dladdr(symbol, &info) != 0 && info.dli_fname
        && strnlen(info.dli_fname, 4097) <= 4096
        && QFileInfo(QString::fromUtf8(info.dli_fname)).canonicalFilePath() == core
        && mappedFile(image, reinterpret_cast<quintptr>(symbol));
}

bool ConsoleSendImage::loaded(pw_impl_module *module) const
{
    if (!module || !admitted()) return false;
    const auto *info = pw_impl_module_get_info(module);
    const QString path(QStringLiteral(QINDAQT_AUDIO_SEND_MODULE_FILE));
    if (!info || !info->filename || strnlen(info->filename, 4097) > 4096
        || QFileInfo(QString::fromUtf8(info->filename)).canonicalFilePath() != path) return false;
    ModuleMapping mapping{.path = path};
    if (!pinnedFile(path, QByteArray(QINDAQT_AUDIO_SEND_MODULE_SHA256), &mapping.identity)) return false;
    dl_iterate_phdr(observeModule, &mapping);
    return mapping.matching == 1 && mapping.mapped;
}
} // namespace QindaQt::Audio
