// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio_platform_p.h"

#include <QtCore/QByteArray>
#include <QtCore/QRegularExpression>
#include <array>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <linux/rfkill.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <utility>

namespace QindaQt::BluetoothRadio {
namespace {
class Fd final {
public:
    explicit Fd(int value = -1) : fd(value) {}
    ~Fd() { if (fd >= 0) close(fd); }
    Fd(const Fd &) = delete;
    Fd &operator=(const Fd &) = delete;
    Fd(Fd &&other) noexcept : fd(std::exchange(other.fd, -1)) {}
    Fd &operator=(Fd &&other) noexcept {
        if (this != &other) {
            if (fd >= 0) close(fd);
            fd = std::exchange(other.fd, -1);
        }
        return *this;
    }
    int fd;
};
bool same(const struct stat &left, const struct stat &right) {
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino
        && (left.st_mode & S_IFMT) == (right.st_mode & S_IFMT);
}
bool sysfsDirectory(int fd, struct stat *identity = nullptr) {
    struct stat metadata{};
    struct statfs filesystem{};
    if (fd < 0 || fstat(fd, &metadata) || !S_ISDIR(metadata.st_mode)
        || fstatfs(fd, &filesystem) || filesystem.f_type != SYSFS_MAGIC) return false;
    if (identity) *identity = metadata;
    return true;
}
QByteArray boundedAttribute(int directory, const char *name) {
    Fd file(openat(directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (file.fd < 0) return {};
    std::array<char, 64> bytes{};
    const auto count = read(file.fd, bytes.data(), bytes.size());
    if (count <= 0 || count >= static_cast<ssize_t>(bytes.size())) return {};
    return QByteArray(bytes.data(), static_cast<qsizetype>(count)).trimmed();
}
bool rfkillDevice(int fd) {
    struct stat info{};
    return fd >= 0 && fstat(fd, &info) == 0 && S_ISCHR(info.st_mode)
        && major(info.st_rdev) == 10 && minor(info.st_rdev) == 242;
}

class LinuxLease final : public RadioLease {
public:
    LinuxLease(Fd hciClass, Fd radioClass, Fd hci, Fd radio, Fd events,
        QByteArray hciName, QByteArray radioName, quint32 index,
        struct stat hciIdentity, struct stat radioIdentity)
        : m_hciClass(std::move(hciClass)), m_radioClass(std::move(radioClass)),
          m_hci(std::move(hci)), m_radio(std::move(radio)), m_events(std::move(events)),
          m_hciName(std::move(hciName)), m_radioName(std::move(radioName)), m_index(index),
          m_hciIdentity(hciIdentity), m_radioIdentity(radioIdentity) {}
    RadioObservation observe() override {
        if (m_retired || !identityCurrent()) return retire();
        // Opening rfkill supplies an initial ADD inventory. Bound every drain;
        // a malformed/overflowing stream cannot produce selected-radio truth.
        for (int count = 0; count < 1024; ++count) {
            rfkill_event event{};
            const auto received = read(m_events.fd, &event, sizeof(event));
            if (received < 0 && errno == EAGAIN) {
                if (!m_seen || !identityCurrent()) return retire();
                return {true, m_soft, m_hard};
            }
            if (received != static_cast<ssize_t>(sizeof(event))
                || event.soft > 1 || event.hard > 1
                || event.op > RFKILL_OP_CHANGE_ALL) return retire();
            if (event.op == RFKILL_OP_CHANGE_ALL) return retire();
            if (event.idx != m_index) continue;
            if (event.type != RFKILL_TYPE_BLUETOOTH || event.op == RFKILL_OP_DEL
                || event.op == RFKILL_OP_CHANGE_ALL
                || (event.op == RFKILL_OP_ADD && m_seen)) return retire();
            if (!m_seen && event.op != RFKILL_OP_ADD) return retire();
            m_seen = true;
            m_soft = event.soft != 0;
            m_hard = event.hard != 0;
        }
        return retire();
    }
    RadioWrite unblock(const std::function<bool()> &current) override {
        if (m_attempted || !observe().current || m_hard || !m_soft)
            return RadioWrite::NotAttempted;
        Fd writer(open("/dev/rfkill", O_WRONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        if (writer.fd < 0 && (errno == EACCES || errno == EPERM))
            return RadioWrite::Denied;
        if (!rfkillDevice(writer.fd) || !identityCurrent())
            return RadioWrite::NotAttempted;
        // The policy must still admit this exact request after opening the
        // writer. Re-observe the pinned kernel objects after that IPC boundary.
        if (!current || !current()) return RadioWrite::Denied;
        if (!observe().current || m_hard || !m_soft) return RadioWrite::NotAttempted;
        // AGENT-GUARD: one selected index/type only. Never CHANGE_ALL: that
        // also changes other radios and future hotplug defaults (ADR-0359).
        // Identity observations cannot make this syscall atomic with hotplug;
        // the owner must revalidate and treat any lost readback as uncertain.
        rfkill_event event{};
        event.idx = m_index;
        event.type = RFKILL_TYPE_BLUETOOTH;
        event.op = RFKILL_OP_CHANGE;
        event.soft = 0;
        m_attempted = true;
        const auto written = write(writer.fd, &event, sizeof(event));
        if (written != static_cast<ssize_t>(sizeof(event))) m_retired = true;
        return RadioWrite::Attempted;
    }
private:
    bool identityCurrent() const {
        struct stat hci{}, radio{}, parent{};
        return fstatat(m_hciClass.fd, m_hciName.constData(), &hci, 0) == 0
            && same(hci, m_hciIdentity)
            && fstatat(m_radioClass.fd, m_radioName.constData(), &radio, 0) == 0
            && same(radio, m_radioIdentity)
            && fstatat(m_radio.fd, "device", &parent, 0) == 0
            && same(parent, m_hciIdentity)
            && boundedAttribute(m_radio.fd, "type") == "bluetooth";
    }
    RadioObservation retire() { m_retired = true; return {}; }
    Fd m_hciClass, m_radioClass, m_hci, m_radio, m_events;
    QByteArray m_hciName, m_radioName;
    quint32 m_index;
    struct stat m_hciIdentity, m_radioIdentity;
    bool m_seen = false, m_retired = false, m_attempted = false;
    bool m_soft = false, m_hard = false;
};

class LinuxPlatform final : public RadioPlatform {
public:
    RadioSelection select(const QString &adapterPath) override {
        static const QRegularExpression path(QStringLiteral("^/org/bluez/(hci(?:0|[1-9][0-9]{0,4}))\\z"));
        const auto match = path.match(adapterPath);
        if (!match.hasMatch()) return {{}, QStringLiteral("radio-stale-target")};
        const auto hciName = match.captured(1).toLatin1();
        Fd hciClass(open("/sys/class/bluetooth", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        Fd radioClass(open("/sys/class/rfkill", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        if (!sysfsDirectory(hciClass.fd) || !sysfsDirectory(radioClass.fd))
            return {{}, QStringLiteral("radio-observation-unavailable")};
        Fd hci(openat(hciClass.fd, hciName.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
        struct stat hciIdentity{};
        if (!sysfsDirectory(hci.fd, &hciIdentity))
            return {{}, QStringLiteral("radio-stale-target")};
        Fd events(open("/dev/rfkill", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
        if (!rfkillDevice(events.fd)) return {{}, QStringLiteral("radio-observation-unavailable")};
        const int directoryCopy = dup(radioClass.fd);
        if (directoryCopy < 0) return {{}, QStringLiteral("radio-observation-unavailable")};
        DIR *entries = fdopendir(directoryCopy);
        if (!entries) {
            close(directoryCopy);
            return {{}, QStringLiteral("radio-observation-unavailable")};
        }
        Fd selected;
        QByteArray selectedName;
        quint32 selectedIndex = 0;
        struct stat selectedIdentity{};
        bool invalid = false;
        unsigned count = 0;
        int enumerationError = 0;
        while (true) {
            errno = 0;
            const dirent *entry = readdir(entries);
            if (!entry) { enumerationError = errno; break; }
            if (++count > 256) { invalid = true; break; }
            const QByteArray name(entry->d_name);
            if (name == "." || name == "..") continue;
            if (!name.startsWith("rfkill") || name.size() > 16) { invalid = true; break; }
            bool validIndex = false;
            const auto index = name.mid(6).toUInt(&validIndex);
            if (!validIndex || name != QByteArray("rfkill") + QByteArray::number(index)) {
                invalid = true; break;
            }
            Fd candidate(openat(radioClass.fd, name.constData(),
                O_RDONLY | O_DIRECTORY | O_CLOEXEC));
            struct stat metadata{}, parent{};
            if (!sysfsDirectory(candidate.fd, &metadata)
                || fstatat(candidate.fd, "device", &parent, 0) != 0) { invalid = true; break; }
            if (!same(parent, hciIdentity)) continue;
            if (boundedAttribute(candidate.fd, "type") != "bluetooth" || selected.fd >= 0) {
                invalid = true; break;
            }
            selected = std::move(candidate);
            selectedName = name;
            selectedIndex = index;
            selectedIdentity = metadata;
        }
        closedir(entries);
        if (invalid || enumerationError) return {{}, QStringLiteral("radio-stale-target")};
        if (selected.fd < 0) return {{}, QStringLiteral("radio-observation-unavailable")};
        return {std::make_unique<LinuxLease>(std::move(hciClass), std::move(radioClass),
            std::move(hci), std::move(selected), std::move(events), hciName, selectedName,
            selectedIndex, hciIdentity, selectedIdentity), {}};
    }
};
} // namespace
std::unique_ptr<RadioPlatform> makeLinuxRadioPlatform() { return std::make_unique<LinuxPlatform>(); }
} // namespace QindaQt::BluetoothRadio
