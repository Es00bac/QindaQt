// SPDX-License-Identifier: GPL-3.0-or-later
#include "private_directory.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <QUuid>
#include <stdexcept>
#include <cerrno>

namespace qindaqt::keyring::service {
namespace {
[[noreturn]] void fail() { throw std::runtime_error("Private storage unavailable"); }
bool safe(const struct stat &s, bool dir) {
    return s.st_uid == geteuid() && (s.st_mode & 07777) == (dir ? 0700 : 0600)
        && (dir ? S_ISDIR(s.st_mode) : (S_ISREG(s.st_mode) && s.st_nlink == 1));
}
QByteArray filename(const QString &name) {
    if (name.isEmpty() || name.size() > 128 || name == "." || name == ".."
        || name.contains('/') || name.contains(QChar(0))) fail();
    return name.toUtf8();
}
}
PrivateDirectory::PrivateDirectory(const QString &path) {
    if (!path.startsWith('/') || path.endsWith('/') || path.contains(QChar(0))) fail();
    int current = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    const auto parts = path.mid(1).split('/');
    for (qsizetype i = 0; i < parts.size(); ++i) {
        if (parts[i].isEmpty() || parts[i] == "." || parts[i] == "..") { close(current); fail(); }
        const auto name = parts[i].toUtf8();
        int next = openat(current, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && errno == ENOENT) {
            if (mkdirat(current, name.constData(), 0700) == 0 && fsync(current) == 0)
                next = openat(current, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        close(current); current = next;
        if (current < 0) fail();
    }
    struct stat s{};
    if (fstat(current, &s) != 0 || !safe(s, true)) { close(current); fail(); }
    directory_ = current;
    lock_ = openat(directory_, ".writer.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600);
    if (lock_ < 0 || fstat(lock_, &s) != 0 || !safe(s, false) || flock(lock_, LOCK_EX | LOCK_NB) != 0) {
        if (lock_ >= 0) close(lock_);
        close(directory_); lock_ = directory_ = -1; fail();
    }
}
PrivateDirectory::~PrivateDirectory() {
    if (lock_ >= 0) close(lock_);
    if (directory_ >= 0) close(directory_);
}
QStringList PrivateDirectory::collections() const {
    DIR *dir = fdopendir(dup(directory_));
    if (!dir) fail();
    QStringList names;
    while (const auto *entry = readdir(dir)) {
        const QString name = QString::fromUtf8(entry->d_name);
        if (name.endsWith(".qkr")) names.append(name.chopped(4));
        if (names.size() > 128) { closedir(dir); fail(); }
    }
    closedir(dir); names.sort(); return names;
}
QByteArray PrivateDirectory::read(const QString &name, int maximum) const {
    const auto file = filename(name);
    const int fd = openat(directory_, file.constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0 && errno == ENOENT) return {};
    struct stat s{};
    if (fd < 0 || fstat(fd, &s) != 0 || !safe(s, false) || s.st_size <= 0 || s.st_size > maximum) {
        if (fd >= 0) close(fd);
        fail();
    }
    QByteArray bytes(static_cast<qsizetype>(s.st_size), Qt::Uninitialized);
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const auto n = ::read(fd, bytes.data() + offset, static_cast<std::size_t>(bytes.size() - offset));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); fail(); }
        offset += n;
    }
    char extra{};
    const bool end = ::read(fd, &extra, 1) == 0;
    close(fd); if (!end) fail();
    return bytes;
}
void PrivateDirectory::replace(const QString &name, const QByteArray &bytes) {
    const auto target = filename(name);
    const auto temporary = (".tmp-" + QUuid::createUuid().toString(QUuid::Id128)).toUtf8();
    struct stat directoryInfo{};
    if (fstat(directory_,&directoryInfo) != 0 || !safe(directoryInfo,true)) throw PersistenceError{};
    struct stat s{};
    if (fstatat(directory_, target.constData(), &s, AT_SYMLINK_NOFOLLOW) == 0 && !safe(s, false)) throw PersistenceError{};
    const int fd = openat(directory_, temporary.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) throw PersistenceError{};
    qsizetype offset = 0;
    bool ok = fchmod(fd, 0600) == 0;
    while (ok && offset < bytes.size()) {
        const auto n = write(fd, bytes.constData() + offset, static_cast<std::size_t>(bytes.size() - offset));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = false; break; }
        offset += n;
    }
    ok = ok && fsync(fd) == 0; close(fd);
    if (!ok || renameat(directory_, temporary.constData(), directory_, target.constData()) != 0) {
        unlinkat(directory_, temporary.constData(), 0); throw PersistenceError{};
    }
    if (fsync(directory_) != 0) throw PersistenceError{}; // Never acknowledge durability uncertainty.
}
void PrivateDirectory::remove(const QString &name) {
    const auto target = filename(name);
    struct stat directoryInfo{};
    if (fstat(directory_,&directoryInfo) != 0 || !safe(directoryInfo,true)) throw PersistenceError{};
    struct stat s{};
    if (fstatat(directory_, target.constData(), &s, AT_SYMLINK_NOFOLLOW) != 0 || !safe(s, false)
        || unlinkat(directory_, target.constData(), 0) != 0 || fsync(directory_) != 0) throw PersistenceError{};
}
}
