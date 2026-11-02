// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_protocol/pairing_math.h>

#include <QtCore/QCryptographicHash>

namespace QindaQt::Link {

QByteArray pairingCommitment(const QByteArray &nd, const QByteArray &spkiD, const QByteArray &spkiP)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView("QLINK-COMMIT-1"));
    hash.addData(nd);
    hash.addData(spkiD);
    hash.addData(spkiP);
    return hash.result();
}

QString pairingCode(const QByteArray &spkiD, const QByteArray &spkiP, const QByteArray &nd,
                    const QByteArray &np)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView("QLINK-SAS-1"));
    hash.addData(spkiD);
    hash.addData(spkiP);
    hash.addData(nd);
    hash.addData(np);
    const QByteArray h = hash.result();
    const quint32 value = (quint32(quint8(h[0])) << 24) | (quint32(quint8(h[1])) << 16)
        | (quint32(quint8(h[2])) << 8) | quint32(quint8(h[3]));
    return QStringLiteral("%1").arg(value % 1000000, 6, 10, QLatin1Char('0'));
}

} // namespace QindaQt::Link
