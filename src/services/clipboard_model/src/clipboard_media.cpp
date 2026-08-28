// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/clipboard_model/clipboard_media.h>

#include <QtCore/QChar>

namespace QindaQt::Services::ClipboardModel {

namespace {

// Deterministic policy tables. Classification is allowlist-based (see the
// contract in clipboard_media.h); the wiki page documents these sets as the
// v1 storage policy.
constexpr std::string_view kSensitiveMedia[] = {
    "application/x-qindaqt-secret",
    "x-kde-passwordmanagerhint",
};
constexpr std::string_view kOneTimeMedia[] = {
    "x-qindaqt-one-time",
};
constexpr std::string_view kStorableMedia[] = {
    "text/plain",
    "text/html",
    "text/uri-list",
    "image/png",
    "image/jpeg",
    "image/bmp",
    "image/gif",
};

bool isAllowedTokenChar(QChar ch)
{
    return (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
        || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
        || ch == QLatin1Char('+') || ch == QLatin1Char('-') || ch == QLatin1Char('.')
        || ch == QLatin1Char('_');
}

bool isAllowedToken(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }
    for (const QChar ch : token) {
        if (!isAllowedTokenChar(ch)) {
            return false;
        }
    }
    return true;
}

} // namespace

MediaCanonicalization canonicalizeMediaType(const QString &mediaType, int maxLength)
{
    MediaCanonicalization result;
    const QString trimmed = mediaType.trimmed();
    if (trimmed.isEmpty()) {
        result.error = ClipboardError::MediaTypeRejected;
        return result;
    }
    if (trimmed.size() > maxLength) {
        result.error = ClipboardError::MediaTypeRejected;
        return result;
    }
    const QString lowered = trimmed.toLower();
    const qsizetype slashCount = lowered.count(QLatin1Char('/'));
    if (slashCount == 0) {
        if (!isAllowedToken(lowered)) {
            result.error = ClipboardError::MediaTypeRejected;
            return result;
        }
    } else if (slashCount == 1) {
        const qsizetype slash = lowered.indexOf(QLatin1Char('/'));
        if (!isAllowedToken(lowered.left(slash)) || !isAllowedToken(lowered.mid(slash + 1))) {
            result.error = ClipboardError::MediaTypeRejected;
            return result;
        }
    } else {
        result.error = ClipboardError::MediaTypeRejected;
        return result;
    }
    result.canonical = lowered;
    return result;
}

bool isCanonicalMediaType(const QString &mediaType, int maxLength)
{
    if (mediaType != mediaType.toLower() || mediaType != mediaType.trimmed()) {
        return false;
    }
    return canonicalizeMediaType(mediaType, maxLength).accepted();
}

MediaClass classifyMediaType(const QString &canonicalMediaType)
{
    if (!isCanonicalMediaType(canonicalMediaType, kMaxMediaTypeLength)) {
        return MediaClass::NonStorable;
    }
    const QByteArray utf8 = canonicalMediaType.toUtf8();
    for (const std::string_view candidate : kSensitiveMedia) {
        if (utf8 == QByteArrayView(candidate.data(), static_cast<qsizetype>(candidate.size()))) {
            return MediaClass::Sensitive;
        }
    }
    for (const std::string_view candidate : kOneTimeMedia) {
        if (utf8 == QByteArrayView(candidate.data(), static_cast<qsizetype>(candidate.size()))) {
            return MediaClass::OneTime;
        }
    }
    for (const std::string_view candidate : kStorableMedia) {
        if (utf8 == QByteArrayView(candidate.data(), static_cast<qsizetype>(candidate.size()))) {
            return MediaClass::Storable;
        }
    }
    return MediaClass::NonStorable;
}

QString sanitizeSourceLabel(const QString &label, int maxCodeUnits)
{
    QString sanitized;
    sanitized.reserve(qMin(label.size(), static_cast<qsizetype>(maxCodeUnits)));
    for (const QChar ch : label) {
        if (sanitized.size() >= maxCodeUnits) {
            break;
        }
        const QChar::Category category = ch.category();
        // Control and format characters (including newlines and bidi marks)
        // become plain spaces so the label can never smuggle markup or
        // terminal escapes into presentation.
        if (category == QChar::Other_Control || category == QChar::Other_Format) {
            sanitized.append(QLatin1Char(' '));
        } else {
            sanitized.append(ch);
        }
    }
    return sanitized;
}

} // namespace QindaQt::Services::ClipboardModel
