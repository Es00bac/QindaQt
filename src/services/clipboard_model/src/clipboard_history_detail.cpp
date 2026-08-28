// SPDX-License-Identifier: LGPL-3.0-or-later

#include "clipboard_history_p.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QtEndian>
#include <QtCore/QString>

#include <qindaqt/services/clipboard_model/clipboard_media.h>

namespace QindaQt::Services::ClipboardModel::HistoryDetail {

qint64 totalFormatBytes(const QList<ClipboardFormat> &formats) noexcept
{
    qint64 total = 0;
    for (const ClipboardFormat &format : formats) {
        total += format.payload.size();
    }
    return total;
}

QByteArray entryFingerprint(const QList<ClipboardFormat> &canonicalFormats)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const ClipboardFormat &format : canonicalFormats) {
        const QByteArray media = format.mediaType.toUtf8();
        // Little-endian length framing keeps media/payload boundaries
        // unambiguous inside the hashed stream.
        char lengthBytes[4];
        qToLittleEndian(static_cast<quint32>(format.payload.size()), lengthBytes);
        hash.addData(QByteArrayView(media));
        hash.addData(QByteArrayView(lengthBytes, 4));
        hash.addData(QByteArrayView(format.payload));
    }
    return hash.result();
}

DerivedPreview derivePreview(const QList<ClipboardFormat> &canonicalFormats, int maxCodeUnits)
{
    for (const ClipboardFormat &format : canonicalFormats) {
        if (format.mediaType != QLatin1String("text/plain")) {
            continue;
        }
        DerivedPreview result;
        QString decoded = QString::fromUtf8(format.payload);
        for (QChar &ch : decoded) {
            const QChar::Category category = ch.category();
            if (category == QChar::Other_Control || category == QChar::Other_Format) {
                ch = QLatin1Char(' ');
            }
        }
        if (decoded.size() > maxCodeUnits) {
            result.preview = decoded.left(maxCodeUnits);
            result.truncated = true;
        } else {
            result.preview = decoded;
            result.truncated = false;
        }
        return result;
    }
    return {};
}

} // namespace QindaQt::Services::ClipboardModel::HistoryDetail
