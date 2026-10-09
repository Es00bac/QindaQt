// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <limits>

namespace QindaQt::Apps::WindowsHandler {
namespace {
constexpr quint64 maximumProgramBytes = 512ULL * 1024 * 1024;

ProfileLaunch refused(const QString &message, const QString &root = {})
{
    ProfileLaunch result; result.message = message; result.profileRoot = root; return result;
}

bool ownedDirectory(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        if (!QDir().mkpath(path) || !QFile::setPermissions(path,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) { return false; }
        info.refresh();
    }
    return info.isDir() && !info.isSymLink() && info.ownerId() == static_cast<quint32>(::getuid())
        && !(info.permissions() & (QFile::WriteGroup | QFile::WriteOther));
}

QProcessEnvironment guestEnvironment(const QProcessEnvironment &session,
    const QString &root, const InstalledWine &runtime)
{
    QProcessEnvironment environment;
    const QStringList keys{QStringLiteral("DISPLAY"), QStringLiteral("WAYLAND_DISPLAY"),
        QStringLiteral("XDG_RUNTIME_DIR"), QStringLiteral("XAUTHORITY"),
        QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("PULSE_SERVER"),
        QStringLiteral("PIPEWIRE_REMOTE"), QStringLiteral("LANG"), QStringLiteral("LC_ALL")};
    for (const QString &key : keys) {
        const QString value = session.value(key);
        if (!value.isEmpty() && value.size() <= 4096 && !value.contains(QChar(0))) {
            environment.insert(key, value);
        }
    }
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));
    environment.insert(QStringLiteral("HOME"), root + QStringLiteral("/home"));
    environment.insert(QStringLiteral("WINEPREFIX"), root + QStringLiteral("/prefix"));
    environment.insert(QStringLiteral("WINESERVER"), runtime.server->path);
    environment.insert(QStringLiteral("WINELOADER"), runtime.loader->path);
    environment.insert(QStringLiteral("WINEDEBUG"), QStringLiteral("-all"));
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), root + QStringLiteral("/home/config"));
    environment.insert(QStringLiteral("XDG_DATA_HOME"), root + QStringLiteral("/home/data"));
    environment.insert(QStringLiteral("XDG_CACHE_HOME"), root + QStringLiteral("/home/cache"));
    environment.insert(QStringLiteral("XDG_STATE_HOME"), root + QStringLiteral("/home/state"));
    return environment;
}
} // namespace

ProfileLaunch launchProgram(const ProgramInspection &inspection,
    const std::shared_ptr<InstalledWine> &runtime, ProgramKind kind,
    const QString &dataRoot, const QProcessEnvironment &session,
    const QString &reuseProfileRoot, const NativeSpawn &spawn,
    const std::shared_ptr<std::atomic_bool> &cancelled)
{
    if (cancelled && cancelled->load()) { return refused(QStringLiteral("Opening was canceled.")); }
    if (!inspection.program || !runtime || !runtime->current()
        || !inspection.program->stillCurrent()) {
        return refused(QStringLiteral("The program or installed runtime changed. Select it again."));
    }
    if (!QDir::isAbsolutePath(dataRoot) || !ownedDirectory(dataRoot)) {
        return refused(QStringLiteral("The Windows profile storage location is unavailable."));
    }
    const QString canonical = inspection.program->canonicalPath();
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
        canonical.toUtf8(), QCryptographicHash::Sha256).toHex());
    QString root = dataRoot + QLatin1Char('/') + key;
    if (!reuseProfileRoot.isEmpty()) {
        const QString existing = QFileInfo(reuseProfileRoot).canonicalFilePath();
        const QString base = QFileInfo(dataRoot).canonicalFilePath();
        if (!existing.startsWith(base + QLatin1Char('/'))
            || !canonical.startsWith(existing + QStringLiteral("/prefix/drive_c/"))) {
            return refused(QStringLiteral("Choose a program inside this Windows profile."));
        }
        root = existing;
    }
    if (!ownedDirectory(root)) { return refused(QStringLiteral("This Windows profile needs repair."), root); }
    const QByteArray lockPath = QFile::encodeName(root + QStringLiteral("/dispatch.lock"));
    const int lock = ::open(lockPath.constData(), O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (lock < 0 || ::flock(lock, LOCK_EX | LOCK_NB) != 0) {
        if (lock >= 0) { ::close(lock); }
        return refused(QStringLiteral("This Windows profile is busy. Close its program before changing or reopening it."), root);
    }
    // The same open-file-description lock is inherited by the bwrap supervisor
    // through slot8. Receipt/profile changes cannot race the checked dispatch.
    struct Lock final { int fd; ~Lock() { ::close(fd); } } heldLock{lock};
    const QString receiptPath = root + QStringLiteral("/registration.json");
    QJsonObject previous;
    if (QFileInfo::exists(receiptPath)) {
        QFile old(receiptPath);
        if (QFileInfo(receiptPath).isSymLink() || !old.open(QIODevice::ReadOnly) || old.size() > 65536) {
            return refused(QStringLiteral("This Windows profile receipt needs repair."), root);
        }
        const QJsonDocument parsed = QJsonDocument::fromJson(old.readAll());
        previous = parsed.object();
        if (!parsed.isObject() || previous.value(QStringLiteral("schema")).toInt() != 1
            || previous.value(QStringLiteral("provenance")).toString() != QStringLiteral("user-selected-file")
            || QUuid(previous.value(QStringLiteral("registrationId")).toString()).isNull()
            || QUuid(previous.value(QStringLiteral("registrationId")).toString()).toString(QUuid::WithoutBraces)
                != previous.value(QStringLiteral("registrationId")).toString()) {
            return refused(QStringLiteral("This Windows profile uses unknown or invalid settings. Its data was retained."), root);
        }
    }
    for (const QString &child : {QStringLiteral("prefix"), QStringLiteral("home"), QStringLiteral("images")}) {
        if (!ownedDirectory(root + QLatin1Char('/') + child)) {
            return refused(QStringLiteral("This Windows profile contains an unsafe folder. Its data was retained."), root);
        }
    }
    const quint64 bytes = inspection.program->identity().bytes;
    if (bytes == 0 || bytes > maximumProgramBytes) {
        return refused(QStringLiteral("This program exceeds the supported 512 MiB image limit."), root);
    }
    const QString launchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString imagePath = root + QStringLiteral("/images/") + launchId + QStringLiteral(".exe");
    QSaveFile image(imagePath);
    if (!image.open(QIODevice::WriteOnly)) { return refused(QStringLiteral("The program image could not be staged."), root); }
    QCryptographicHash digest(QCryptographicHash::Sha256); QByteArray buffer(1024 * 1024, '\0');
    quint64 copied = 0; QElapsedTimer elapsed; elapsed.start();
    while (copied < bytes) {
        if (cancelled && cancelled->load()) {
            return refused(QStringLiteral("Opening was canceled. Existing profile data was retained."), root);
        }
        const size_t wanted = static_cast<size_t>(qMin<quint64>(bytes - copied,
            static_cast<quint64>(buffer.size())));
        const ssize_t count = ::pread(inspection.program->borrowedDescriptor(), buffer.data(), wanted,
            static_cast<off_t>(copied));
        if (count <= 0 || elapsed.elapsed() > 30000
            || image.write(buffer.constData(), count) != count) {
            return refused(QStringLiteral("The program image could not be read or staged in time. Its profile data was retained."), root);
        }
        digest.addData(QByteArrayView(buffer.constData(), count)); copied += static_cast<quint64>(count);
    }
    if (!inspection.program->stillCurrent() || !image.commit()
        || !QFile::setPermissions(imagePath, QFile::ReadOwner)) {
        return refused(QStringLiteral("The selected program changed during preparation. Its profile data was retained."), root);
    }
    // User images use the same stable descriptor verifier as root runtime files,
    // with a user owner and a larger declared bound; never claim Portage origin.
    const QByteArray sourceDigest = digest.result();
    const QString cachedImage = root + QStringLiteral("/images/") + QString::fromLatin1(sourceDigest.toHex()) + QStringLiteral(".exe");
    if (QFileInfo::exists(cachedImage)) {
        const auto cached = pinFile(cachedImage, static_cast<quint32>(::getuid()), {}, maximumProgramBytes);
        if (!cached || cached->digest != sourceDigest) {
            return refused(QStringLiteral("This Windows profile contains a changed program image. Its data was retained."), root);
        }
        // Only this operation's fresh temporary snapshot is removed. Existing
        // program images, prefix, receipts and user data are never cleanup.
        if (!QFile::remove(imagePath)) { return refused(QStringLiteral("The temporary program image could not be finalized."), root); }
    } else if (!QFile::rename(imagePath, cachedImage)) {
        return refused(QStringLiteral("The program image could not be finalized."), root);
    }
    auto imagePin = pinFile(cachedImage, static_cast<quint32>(::getuid()), {}, maximumProgramBytes);
    if (!imagePin || imagePin->digest != digest.result()) {
        return refused(QStringLiteral("The staged program image failed verification."), root);
    }
    const QString registrationId = previous.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : previous.value(QStringLiteral("registrationId")).toString();
    const QString previousRevision = previous.value(QStringLiteral("revision")).toString();
    bool revisionValid = false; const quint64 revision = previous.isEmpty() ? 0 : previousRevision.toULongLong(&revisionValid);
    if ((!previous.isEmpty() && (!revisionValid || revision == 0)) || revision == std::numeric_limits<quint64>::max()) {
        return refused(QStringLiteral("This Windows profile revision needs repair. Its data was retained."), root);
    }
    const QString requestedKind = kind == ProgramKind::Installer ? QStringLiteral("installer") : QStringLiteral("portable");
    const bool sameRecipe = !previous.isEmpty()
        && previous.value(QStringLiteral("sourcePath")).toString() == canonical
        && previous.value(QStringLiteral("sourceSha256")).toString() == QString::fromLatin1(imagePin->digest.toHex())
        && previous.value(QStringLiteral("runtimeCpv")).toString() == runtime->id
        && previous.value(QStringLiteral("runtimeSha256")).toString() == QString::fromLatin1(runtime->loader->digest.toHex())
        && previous.value(QStringLiteral("kind")).toString() == requestedKind;
    QJsonObject receipt{{QStringLiteral("schema"), 1},
        {QStringLiteral("provenance"), QStringLiteral("user-selected-file")},
        {QStringLiteral("registrationId"), registrationId},
        {QStringLiteral("revision"), QString::number(sameRecipe ? revision : revision + 1)},
        {QStringLiteral("sourcePath"), canonical},
        {QStringLiteral("sourceSha256"), QString::fromLatin1(imagePin->digest.toHex())},
        {QStringLiteral("runtimeCpv"), runtime->id},
        {QStringLiteral("runtimeSha256"), QString::fromLatin1(runtime->loader->digest.toHex())},
        {QStringLiteral("kind"), requestedKind},
        {QStringLiteral("exposure"), QStringLiteral("native-user-session")},
        {QStringLiteral("lastLaunchId"), launchId}, {QStringLiteral("phase"), QStringLiteral("prepared")}};
    QSaveFile record(receiptPath);
    const QByteArray encoded = QJsonDocument(receipt).toJson(QJsonDocument::Compact);
    if (!record.open(QIODevice::WriteOnly) || record.write(encoded) != encoded.size()
        || !record.commit() || !QFile::setPermissions(receiptPath, QFile::ReadOwner | QFile::WriteOwner)) {
        return refused(QStringLiteral("The Windows profile receipt could not be saved."), root);
    }
    // Transaction held here: revalidate all source/runtime handles and profile
    // receipt immediately before the only submission. No copied GUI snapshot
    // authorizes a later recipe replacement or implicit replay.
    if ((cancelled && cancelled->load()) || !inspection.program->stillCurrent()
        || !runtime->current() || !imagePin->current()) {
        return refused(QStringLiteral("The program or runtime changed before dispatch."), root);
    }
    QStringList arguments{QStringLiteral("--new-session"), QStringLiteral("--unshare-user"),
        QStringLiteral("--unshare-ipc"), QStringLiteral("--unshare-uts"),
        QStringLiteral("--bind"), QStringLiteral("/"), QStringLiteral("/"),
        QStringLiteral("--ro-bind"), QStringLiteral("/proc/self/fd/3"), canonical,
        QStringLiteral("--ro-bind"), QStringLiteral("/proc/self/fd/4"), runtime->preloader->path,
        QStringLiteral("--ro-bind"), QStringLiteral("/proc/self/fd/5"), runtime->loader->path,
        QStringLiteral("--ro-bind"), QStringLiteral("/proc/self/fd/6"), runtime->server->path,
        QStringLiteral("--preserve-fds"), QStringLiteral("6"),
        QStringLiteral("--chdir"), QFileInfo(canonical).absolutePath()};
    const QProcessEnvironment environment = guestEnvironment(session, root, *runtime);
    arguments.append(QStringLiteral("--clearenv"));
    for (const QString &keyName : environment.keys()) {
        arguments.append({QStringLiteral("--setenv"), keyName, environment.value(keyName)});
    }
    arguments.append({QStringLiteral("--"), runtime->preloader->path, runtime->loader->path, canonical});
    const QVector<int> descriptors{imagePin->descriptor, runtime->preloader->descriptor,
        runtime->loader->descriptor, runtime->server->descriptor, runtime->bubblewrap->descriptor, lock};
    ProfileLaunch result; result.profileRoot = root; result.registrationId = registrationId; result.launchId = launchId;
    const int status = spawn ? spawn(arguments, environment, descriptors, result.processId)
        : spawnPinnedWine(arguments, environment, descriptors, result.processId);
    if (status != 0) {
        result.message = QStringLiteral("The Windows runtime could not be submitted. Its profile and data were retained.");
        return result;
    }
    result.submitted = true;
    result.outcomeUnknown = cancelled && cancelled->load();
    result.message = kind == ProgramKind::Installer
        ? QStringLiteral("Installer startup submitted. Finish installation, then choose the installed program.")
        : QStringLiteral("Windows program startup submitted.");
    return result;
}
} // namespace QindaQt::Apps::WindowsHandler
