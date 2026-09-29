// SPDX-License-Identifier: GPL-3.0-or-later
#include "atomic_file_p.h"
#include "format_p.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <utility>
#include <sys/syscall.h>
#include <linux/fs.h>

namespace qindaqt::keyring::detail {
namespace {
class Fd {
public:
    explicit Fd(int fd = -1) : value_(fd) {}
    ~Fd() { if (value_ >= 0) close(value_); }
    Fd(Fd &&other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Fd &operator=(Fd &&other) noexcept {
        if (this != &other) {
            if (value_ >= 0) close(value_);
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    int get() const { return value_; }
private:
    int value_;
};
bool secureStat(const struct stat &s, bool directory) {
    return s.st_uid == geteuid() && (s.st_mode & 07777) == (directory ? 0700 : 0600)
        && (directory ? S_ISDIR(s.st_mode) : (S_ISREG(s.st_mode) && s.st_nlink == 1));
}
Fd openDirectory(const std::string &path) {
    if (path.empty() || path[0] != '/' || path.back() == '/' || path.find('\0') != std::string::npos)
        return Fd{};
    Fd fd(open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    std::size_t start = 1;
    while (start < path.size()) {
        const auto end = path.find('/', start);
        const auto part = path.substr(start, end == std::string::npos ? path.size() - start : end - start);
        if (part.empty() || part == "." || part == "..") return Fd{};
        const bool leaf = end == std::string::npos;
        int next = openat(fd.get(), part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && errno == ENOENT && leaf) {
            if (mkdirat(fd.get(), part.c_str(), 0700) != 0 || fsync(fd.get()) != 0) return Fd{};
            next = openat(fd.get(), part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            // Parent fsync commits the new leaf; save later fsyncs its file entries.
        }
        fd = Fd(next);
        if (fd.get() < 0) return Fd{};
        if (leaf) {
            struct stat s{};
            if (fstat(fd.get(), &s) != 0 || !secureStat(s, true)) return Fd{};
        }
        if (leaf) return fd;
        start = end + 1;
    }
    return Fd{};
}
bool existingTargetSafe(int dir, const std::string &name) {
    struct stat s{};
    if (fstatat(dir, name.c_str(), &s, AT_SYMLINK_NOFOLLOW) != 0) return errno == ENOENT;
    return secureStat(s, false);
}
}
StoreError readFile(const std::string &directory, const std::string &name, Bytes &bytes) {
    if (!validId(name)) return StoreError::InvalidInput;
    Fd dir = openDirectory(directory);
    if (dir.get() < 0) return StoreError::IoError;
    Fd fd(openat(dir.get(), name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    struct stat s{};
    if (fd.get() < 0 || fstat(fd.get(), &s) != 0 || !secureStat(s, false)
        || s.st_size < 0 || static_cast<unsigned long long>(s.st_size) > MaxFile) return StoreError::IoError;
    Bytes candidate(static_cast<std::size_t>(s.st_size));
    std::size_t offset = 0;
    while (offset < candidate.size()) {
        const auto n = read(fd.get(), candidate.data() + offset, candidate.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return StoreError::IoError;
        offset += static_cast<std::size_t>(n);
    }
    unsigned char extra{};
    if (read(fd.get(), &extra, 1) != 0) return StoreError::IoError;
    bytes = std::move(candidate);
    return StoreError::None;
}
StoreError replaceFile(const std::string &directory, const std::string &name,
                       std::span<const unsigned char> bytes, const std::function<bool()> &barrier, bool mustBeAbsent) {
    if (!validId(name) || bytes.size() > MaxFile) return StoreError::InvalidInput;
    Fd dir = openDirectory(directory);
    if (dir.get() < 0 || !existingTargetSafe(dir.get(), name)) return StoreError::IoError;
    std::array<unsigned char, 16> entropy{};
    if (!random(entropy)) return StoreError::CryptoUnavailable;
    constexpr char hex[] = "0123456789abcdef";
    std::string temporary = ".tmp-";
    for (const auto byte : entropy) {
        temporary += hex[byte >> 4]; temporary += hex[byte & 15];
    }
    Fd fd(openat(dir.get(), temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (fd.get() < 0) return StoreError::IoError;
    auto fail = [&] { unlinkat(dir.get(), temporary.c_str(), 0); return StoreError::IoError; };
    if (fchmod(fd.get(), 0600) != 0) return fail();
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto n = write(fd.get(), bytes.data() + offset, bytes.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return fail();
        offset += static_cast<std::size_t>(n);
    }
    if (fsync(fd.get()) != 0) return fail();
    try { if (barrier && !barrier()) return fail(); }
    catch (...) { return fail(); }
    if (!existingTargetSafe(dir.get(), name)
        || syscall(SYS_renameat2, dir.get(), temporary.c_str(), dir.get(), name.c_str(),
                   mustBeAbsent ? RENAME_NOREPLACE : 0) != 0) return fail();
    // AGENT-CONTRACT: after rename failure of parent fsync is not rollback;
    // callers receive DurabilityUnknown and must reload, never assert old bytes.
    return fsync(dir.get()) == 0 ? StoreError::None : StoreError::DurabilityUnknown;
}
}
