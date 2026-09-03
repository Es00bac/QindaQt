// SPDX-License-Identifier: GPL-3.0-or-later

#include "upstream_identity.h"

#include <QCryptographicHash>

namespace QindaQt::Power::Upstream {
namespace {

// 32 hex characters = 32 UTF-8 bytes, comfortably inside the 128-byte
// Power1 opaque-ID bound while keeping collision odds negligible.
constexpr qsizetype kOpaqueIdHexLength = 32;

} // namespace

QString deriveOpaqueId(const QString &domain, const QString &key)
{
    const QByteArray material =
        domain.toUtf8() + QByteArrayLiteral("|") + key.toUtf8();
    const QByteArray digest =
        QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(digest.left(kOpaqueIdHexLength));
}

} // namespace QindaQt::Power::Upstream
