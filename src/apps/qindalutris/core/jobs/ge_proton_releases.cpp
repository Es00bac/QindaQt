// SPDX-License-Identifier: GPL-3.0-or-later
#include "ge_proton_releases.h"

#include "download_allowlist.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>

namespace QindaQt::QindaLutris {

namespace {

const QString kTarSuffix = QStringLiteral(".tar.gz");
const QString kSumSuffix = QStringLiteral(".sha512sum");

struct Asset {
  QString name;
  QUrl url;
  qint64 size = -1;
};

// The asset must sit at exactly the upstream download URL for its tag.
std::optional<Asset> findAsset(const QJsonArray &assets, const QString &tag, const QString &name) {
  for (const QJsonValue &value : assets) {
    const QJsonObject asset = value.toObject();
    if (asset.value(QStringLiteral("name")).toString() != name) {
      continue;
    }
    const QString urlText = asset.value(QStringLiteral("browser_download_url")).toString();
    const QUrl expected = geProtonAssetUrl(tag, name);
    if (!isAllowedDownloadUrl(urlText) || QUrl(urlText, QUrl::StrictMode) != expected) {
      return std::nullopt;
    }
    Asset found;
    found.name = name;
    found.url = expected;
    found.size = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble(-1));
    return found;
  }
  return std::nullopt;
}

bool isHex128(const QByteArray &text) {
  if (text.size() != 128) {
    return false;
  }
  for (const char c : text) {
    const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                     (c >= 'A' && c <= 'F');
    if (!hex) {
      return false;
    }
  }
  return true;
}

} // namespace

QUrl geProtonReleasesApiUrl(int perPage) {
  QUrl url(QStringLiteral(
      "https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("per_page"),
                     QString::number(std::clamp(perPage, 1, kMaxReleases)));
  url.setQuery(query);
  return url;
}

bool isSafeToolName(const QString &name) {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$"));
  return name != QLatin1String(".") && name != QLatin1String("..") &&
         pattern.match(name).hasMatch();
}

GeProtonReleaseList parseGeProtonReleases(const QByteArray &json,
                                          const QString &architecture) {
  GeProtonReleaseList out;
  if (json.size() > kMaxReleaseDocumentBytes) {
    out.error = QStringLiteral("The release list was larger than expected.");
    return out;
  }
  QJsonParseError parseError{};
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    out.error = QStringLiteral("The release list could not be read.");
    return out;
  }
  const QJsonArray releases = document.array();
  for (const QJsonValue &value : releases) {
    if (out.releases.size() >= kMaxReleases) {
      break;
    }
    const QJsonObject object = value.toObject();
    if (object.value(QStringLiteral("draft")).toBool(false)) {
      continue;
    }
    GeProtonRelease release;
    release.tagName = object.value(QStringLiteral("tag_name")).toString();
    if (!release.tagName.startsWith(QLatin1String("GE-Proton")) ||
        !isSafeToolName(release.tagName)) {
      continue;
    }
    const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
    QStringList stems{release.tagName + QLatin1Char('-') + architecture};
    if (architecture == QLatin1String("x86_64")) {
      stems.append(release.tagName); // pre-architecture naming
    }
    for (const QString &stem : stems) {
      if (!isSafeToolName(stem)) {
        continue;
      }
      const auto tarball = findAsset(assets, release.tagName, stem + kTarSuffix);
      const auto checksum = findAsset(assets, release.tagName, stem + kSumSuffix);
      if (!tarball || !checksum) {
        continue;
      }
      release.toolName = stem;
      release.tarballName = tarball->name;
      release.tarballUrl = tarball->url;
      release.tarballBytes = tarball->size;
      release.checksumName = checksum->name;
      release.checksumUrl = checksum->url;
      break;
    }
    if (release.toolName.isEmpty()) {
      continue;
    }
    release.publishedAt = QDateTime::fromString(
        object.value(QStringLiteral("published_at")).toString(), Qt::ISODate);
    release.prerelease = object.value(QStringLiteral("prerelease")).toBool(false);
    out.releases.append(release);
  }
  out.ok = true;
  return out;
}

std::optional<QByteArray> parseSha512SumFile(const QByteArray &content,
                                             const QString &fileName) {
  if (content.size() > 64 * 1024) {
    return std::nullopt;
  }
  const QByteArray wanted = fileName.toUtf8();
  const QList<QByteArray> lines = content.split('\n');
  for (QByteArray line : lines) {
    line = line.trimmed();
    const qsizetype space = line.indexOf(' ');
    if (space <= 0) {
      continue;
    }
    const QByteArray hash = line.left(space);
    QByteArray name = line.mid(space).trimmed();
    if (name.startsWith('*')) {
      name.remove(0, 1);
    }
    if (name == wanted && isHex128(hash)) {
      return hash.toLower();
    }
  }
  return std::nullopt;
}

std::optional<QByteArray> sha512HexOfFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  QCryptographicHash hash(QCryptographicHash::Sha512);
  if (!hash.addData(&file)) {
    return std::nullopt;
  }
  return hash.result().toHex();
}

QUrl geProtonAssetUrl(const QString &tag, const QString &fileName) {
  return QUrl(QString::fromLatin1(kGeProtonDownloadPrefix) + tag + QLatin1Char('/') + fileName,
              QUrl::StrictMode);
}

bool isUpstreamGeProtonRelease(const GeProtonRelease &release) {
  const QString &tag = release.tagName;
  if (!tag.startsWith(QLatin1String("GE-Proton")) || !isSafeToolName(tag)) {
    return false;
  }
  static const QStringList suffixes{QStringLiteral("-x86_64"), QStringLiteral("-aarch64"), QString()};
  bool named = false;
  for (const QString &suffix : suffixes) {
    named = named || release.toolName == tag + suffix;
  }
  return named && isSafeToolName(release.toolName) &&
         release.tarballName == release.toolName + kTarSuffix &&
         release.checksumName == release.toolName + kSumSuffix &&
         release.tarballUrl == geProtonAssetUrl(tag, release.tarballName) &&
         release.checksumUrl == geProtonAssetUrl(tag, release.checksumName) &&
         isAllowedDownloadUrl(release.tarballUrl) && isAllowedDownloadUrl(release.checksumUrl);
}

} // namespace QindaQt::QindaLutris
