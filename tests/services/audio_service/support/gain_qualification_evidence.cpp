// SPDX-License-Identifier: GPL-3.0-or-later
#include "gain_qualification_evidence.h"
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cmath>

namespace QindaQt::Audio::Test
{
namespace {
bool sameVersion(const struct stat &a, const struct stat &b)
{
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size
        && a.st_mode == b.st_mode && a.st_nlink == b.st_nlink
        && a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec
        && a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
} // namespace
bool gainExactKeys(const QJsonObject &object, const QSet<QString> &keys)
{
    const auto names = object.keys();
    return QSet<QString>(names.begin(), names.end()) == keys;
}
bool gainInteger(const QJsonValue &value, const qint64 minimum, const qint64 maximum)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number
        && number >= static_cast<double>(minimum) && number <= static_cast<double>(maximum);
}
QByteArray gainReadEvidence(const GainImageArtifact &artifact, const qint64 limit)
{
    static const QRegularExpression shape(QStringLiteral("^[0-9a-f]{64}$"));
    if (artifact.path.isEmpty() || artifact.path.size() > 4096
        || QFileInfo(artifact.path).canonicalFilePath() != artifact.path
        || artifact.bytes <= 0 || artifact.bytes > limit
        || !shape.match(QString::fromLatin1(artifact.sha256)).hasMatch())
        return {};
    const auto path = QFile::encodeName(artifact.path);
    const int fd = ::open(path.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return {};
    struct stat before{}, after{}, named{};
    bool accepted = ::fstat(fd, &before) == 0 && S_ISREG(before.st_mode)
        && before.st_nlink == 1 && !(before.st_mode & (S_IWGRP | S_IWOTH))
        && before.st_size == artifact.bytes;
    QByteArray data;
    char buffer[64 * 1024];
    while (accepted) {
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { accepted = false; break; }
        if (count == 0) break;
        data.append(buffer, static_cast<qsizetype>(count));
        if (data.size() > artifact.bytes) { accepted = false; break; }
    }
    accepted = accepted && data.size() == artifact.bytes
        && QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() == artifact.sha256
        && ::fstat(fd, &after) == 0 && sameVersion(before, after)
        && ::lstat(path.constData(), &named) == 0 && sameVersion(after, named);
    ::close(fd);
    return accepted ? data : QByteArray{};
}
bool gainEvidenceRecord(const QJsonValue &value, const QString &snapshotRoot,
                        GainImageArtifact *artifact, QByteArray *data)
{
    const auto record = value.toObject();
    if (!value.isObject() || !gainExactKeys(record, {QStringLiteral("path"),
            QStringLiteral("sha256"), QStringLiteral("bytes")})
        || !record.value(QStringLiteral("path")).isString()
        || !record.value(QStringLiteral("sha256")).isString()
        || !gainInteger(record.value(QStringLiteral("bytes")), 1, 16 * 1024 * 1024))
        return false;
    GainImageArtifact candidate{.path = record.value(QStringLiteral("path")).toString(),
        .sha256 = record.value(QStringLiteral("sha256")).toString().toLatin1(),
        .bytes = record.value(QStringLiteral("bytes")).toInteger()};
    if (!candidate.path.startsWith(snapshotRoot + QLatin1Char('/'))) return false;
    const auto observed = gainReadEvidence(candidate);
    if (observed.isEmpty()) return false;
    *artifact = candidate;
    *data = observed;
    return true;
}
} // namespace QindaQt::Audio::Test
