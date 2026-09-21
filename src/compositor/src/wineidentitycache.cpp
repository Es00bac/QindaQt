// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/wineidentitycache.h"

#include "qindaqt/compositor/peicon.h"
#include "qindaqt/compositor/steamappidentity.h"
#include "qindaqt/compositor/winelaunchpaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace QindaQt::Compositor {
namespace {

constexpr qint64 kMaxSignatureFileBytes = 4096;

[[nodiscard]] QString joinPath(const QString &base, const QString &leaf)
{
    return base + QLatin1Char('/') + leaf;
}

} // namespace

WineIdentityCache::WineIdentityCache(QString cacheRoot, QString homeDir)
    : m_cacheRoot(QDir::cleanPath(cacheRoot)), m_homeDir(std::move(homeDir))
{
}

QString WineIdentityCache::defaultCacheRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/qindaqt/wine-icons");
}

QString WineIdentityCache::cacheIconNameForApplicationId(const QString &applicationId)
{
    // AGENT-CONTRACT: byte-identical to
    // QindaQt::Shell::Icons::IconRuntime::wineCacheIconNameForAppId. The
    // grammar is the freedesktop icon-name subset the shell locator confines
    // to ([a-z0-9._-], no ".."); anything outside maps to '-', a `.exe`
    // suffix is dropped, and the result carries the reserved prefix so it can
    // never shadow a real themed icon.
    QString base = applicationId.trimmed();
    if (base.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        base.chop(4);
    }
    QString mapped;
    mapped.reserve(base.size());
    for (const QChar character : base.toLower()) {
        const char16_t code = character.unicode();
        const bool acceptable = (code >= 'a' && code <= 'z')
            || (code >= '0' && code <= '9') || character == QLatin1Char('_')
            || character == QLatin1Char('-') || character == QLatin1Char('.');
        mapped.append(acceptable ? character : QLatin1Char('-'));
    }
    const QString prefix = QStringLiteral("qindaqt-wine-");
    if (mapped.isEmpty() || mapped.contains(QStringLiteral(".."))) {
        return {};
    }
    // The shell locator refuses names over 128 UTF-8 bytes; the mapping is
    // pure ASCII, so character count equals byte count.
    const qsizetype budget = 128 - prefix.size();
    if (mapped.size() > budget) {
        return {};
    }
    return prefix + mapped;
}

QString WineIdentityCache::steamNameForClass(const QString &resourceClass)
{
    const std::optional<quint64> id = steamAppIdFromClass(resourceClass);
    if (!id.has_value()) {
        return {};
    }
    const QStringList roots = steamLibraryRoots();
    const QString manifestName =
        QStringLiteral("appmanifest_%1.acf").arg(*id);

    const auto cached = m_steamNames.constFind(*id);
    if (cached != m_steamNames.cend()) {
        // Revalidate by stat; a rewritten manifest reparses, a removed one
        // drops the cached name so the window falls back to ADR-0169.
        if (sameFile(cached->source, signatureOf(cached->source.path))) {
            return cached->name;
        }
        m_steamNames.remove(*id);
    }

    for (const QString &root : roots) {
        const QString path = joinPath(root, manifestName);
        const FileSignature signature = signatureOf(path);
        if (!signature.exists) {
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray payload = file.read(kMaxVdfBytes + 1);
        const QString name = parseSteamAppManifestName(payload);
        // The map is keyed by real windows' ids, so it is naturally tiny;
        // the cap just keeps a pathological session from growing it.
        if (m_steamNames.size() < 256) {
            m_steamNames.insert(*id, SteamNameEntry{name, signature});
        }
        return name;
    }
    // Misses are deliberately not cached: a game installed later must appear
    // on the next query, and the bounded stats above are the whole cost.
    return {};
}

void WineIdentityCache::ensureIconForClient(const QByteArray &cmdline,
                                            const QByteArray &environBytes)
{
    const QString windowsPath = windowsExecutablePathFromCommandLine(cmdline);
    if (windowsPath.isEmpty()) {
        return;
    }
    const QString prefix = winePrefixFromEnviron(environBytes, m_homeDir);
    if (prefix.isEmpty()) {
        return;
    }
    const QString hostPath = hostPathForWindowsExecutable(windowsPath, prefix);
    if (hostPath.isEmpty()) {
        return;
    }

    // Containment is re-checked after symlink resolution for paths the C:
    // mapping placed beneath the prefix; a canonical escape fails closed.
    // Z: and POSIX-passthrough paths are the user's own session naming host
    // files directly, bounded by the PE parser's own ceilings.
    const bool underPrefix = windowsPath.size() >= 2
        && windowsPath.at(1) == QLatin1Char(':')
        && windowsPath.at(0).toUpper() == QLatin1Char('C');
    const QFileInfo hostInfo(hostPath);
    const QString canonicalHost = hostInfo.canonicalFilePath();
    if (canonicalHost.isEmpty() || !hostInfo.isFile()) {
        return;
    }
    if (underPrefix) {
        const QString canonicalPrefix = QFileInfo(prefix).canonicalFilePath();
        if (canonicalPrefix.isEmpty()
            || !canonicalHost.startsWith(canonicalPrefix + QLatin1Char('/'))) {
            return;
        }
    }

    const QString iconName =
        cacheIconNameForApplicationId(hostInfo.fileName());
    if (iconName.isEmpty()) {
        return;
    }
    const QString iconPath = joinPath(m_cacheRoot, iconName + QStringLiteral(".png"));
    const QString signaturePath =
        joinPath(m_cacheRoot, iconName + QStringLiteral(".sig"));
    const FileSignature source = signatureOf(canonicalHost);

    // Signature sidecar: source path, mtime, size. A hit costs one bounded
    // read; a miss (or any mismatch) re-extracts.
    const QString expectedSignature = canonicalHost + QLatin1Char('\n')
        + QString::number(source.mtimeMs) + QLatin1Char('\n')
        + QString::number(source.size) + QLatin1Char('\n');
    QFile signatureFile(signaturePath);
    if (signatureFile.open(QIODevice::ReadOnly)) {
        const QByteArray current = signatureFile.read(kMaxSignatureFileBytes);
        signatureFile.close();
        if (current == expectedSignature.toUtf8()
            && QFileInfo::exists(iconPath)) {
            return;
        }
    }

    QFile executable(canonicalHost);
    if (!executable.open(QIODevice::ReadOnly)) {
        return;
    }
    const QImage icon = extractPeIcon(executable, source.size, kWineIconTargetSize);
    if (icon.isNull()) {
        // Fail closed: a stale pair naming a different (or changed) source is
        // wrong identity, so it is removed rather than shown.
        if (QFileInfo::exists(signaturePath)) {
            QFile::remove(signaturePath);
            QFile::remove(iconPath);
        }
        return;
    }

    QDir().mkpath(m_cacheRoot);
    // Write both artifacts through temp+rename so the shell never observes a
    // half-written icon.
    const QString temporaryIcon =
        iconPath + QStringLiteral(".tmp.%1").arg(qintptr(QCoreApplication::applicationPid()));
    if (!icon.save(temporaryIcon, "PNG")) {
        QFile::remove(temporaryIcon);
        return;
    }
    if (!QFile::rename(temporaryIcon, iconPath)) {
        // Some filesystems refuse rename-over-existing; retry once after
        // removing the stale target.
        QFile::remove(iconPath);
        if (!QFile::rename(temporaryIcon, iconPath)) {
            QFile::remove(temporaryIcon);
            return;
        }
    }
    const QString temporarySignature = signaturePath + QStringLiteral(".tmp");
    QFile signatureOut(temporarySignature);
    if (signatureOut.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        signatureOut.write(expectedSignature.toUtf8());
        signatureOut.close();
        if (!QFile::rename(temporarySignature, signaturePath)) {
            QFile::remove(signaturePath);
            if (!QFile::rename(temporarySignature, signaturePath)) {
                QFile::remove(temporarySignature);
            }
        }
    } else {
        QFile::remove(temporarySignature);
    }
}

WineIdentityCache::FileSignature WineIdentityCache::signatureOf(const QString &path)
{
    const QFileInfo info(path);
    FileSignature signature;
    signature.path = path;
    signature.exists = info.isFile();
    if (signature.exists) {
        signature.mtimeMs = info.lastModified().toMSecsSinceEpoch();
        signature.size = info.size();
    }
    return signature;
}

bool WineIdentityCache::sameFile(const FileSignature &a, const FileSignature &b)
{
    return a.exists && b.exists && a.mtimeMs == b.mtimeMs && a.size == b.size;
}

QStringList WineIdentityCache::steamLibraryRoots()
{
    if (m_steamRootsValid && !steamRootsStale()) {
        return m_steamRoots;
    }
    rediscoverSteamRoots();
    return m_steamRoots;
}

bool WineIdentityCache::steamRootsStale() const
{
    const QString candidateA =
        joinPath(m_homeDir, QStringLiteral(".steam/steam/steamapps"));
    const QString candidateB =
        joinPath(m_homeDir, QStringLiteral(".local/share/Steam/steamapps"));
    const FileSignature foldersA =
        signatureOf(joinPath(candidateA, QStringLiteral("libraryfolders.vdf")));
    const FileSignature foldersB =
        signatureOf(joinPath(candidateB, QStringLiteral("libraryfolders.vdf")));
    const auto changed = [](const FileSignature &now, const FileSignature &cached) {
        if (now.exists != cached.exists) {
            return true;
        }
        if (!now.exists) {
            return false;
        }
        return now.mtimeMs != cached.mtimeMs || now.size != cached.size;
    };
    return changed(foldersA, m_foldersSignatureA)
        || changed(foldersB, m_foldersSignatureB);
}

void WineIdentityCache::rediscoverSteamRoots()
{
    const QStringList candidates{
        joinPath(m_homeDir, QStringLiteral(".steam/steam/steamapps")),
        joinPath(m_homeDir, QStringLiteral(".local/share/Steam/steamapps")),
    };
    m_foldersSignatureA =
        signatureOf(joinPath(candidates.at(0), QStringLiteral("libraryfolders.vdf")));
    m_foldersSignatureB =
        signatureOf(joinPath(candidates.at(1), QStringLiteral("libraryfolders.vdf")));

    QStringList roots;
    const auto admit = [&roots](const QString &path) {
        if (path.isEmpty()) {
            return;
        }
        const QString canonical = QDir(path).canonicalPath();
        const QString cleaned =
            canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
        // AGENT-GUARD: a declared root must stay a plausible steamapps tree;
        // the vdf reader already bounded the text, and this module never
        // enumerates the directory - it only ever stats appmanifest_<id>.acf
        // beneath it.
        if (!roots.contains(cleaned)
            && roots.size() < kMaxSteamLibraryRoots + 2) {
            roots.append(cleaned);
        }
    };
    for (const QString &candidate : candidates) {
        admit(candidate);
    }
    for (const QString &candidate : candidates) {
        const QString foldersPath =
            joinPath(candidate, QStringLiteral("libraryfolders.vdf"));
        QFile file(foldersPath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray payload = file.read(kMaxVdfBytes + 1);
        QStringList declared;
        if (parseSteamLibraryFolders(payload, &declared)) {
            for (const QString &library : std::as_const(declared)) {
                admit(joinPath(library, QStringLiteral("steamapps")));
            }
        }
    }
    m_steamRoots = roots;
    m_steamRootsValid = true;
}

} // namespace QindaQt::Compositor
