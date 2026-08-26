// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_access_token.h"

#include <QRandomGenerator>

#include <cstring>
#include <utility>

namespace QindaQt::Services::NotificationPresentation {

PresentationAccessToken::PresentationAccessToken(QByteArray hexValue)
    : m_hexValue(std::move(hexValue))
{
}

PresentationAccessToken PresentationAccessToken::generate()
{
    QByteArray randomBytes(32, '\0');
    for (qsizetype offset = 0; offset < randomBytes.size();
         offset += qsizetype(sizeof(quint32))) {
        const quint32 value = QRandomGenerator::system()->generate();
        std::memcpy(randomBytes.data() + offset, &value, sizeof(value));
    }
    return PresentationAccessToken(randomBytes.toHex());
}

std::optional<PresentationAccessToken> PresentationAccessToken::fromHex(
    QStringView value,
    QString *error)
{
    if (value.size() != 64) {
        if (error != nullptr) {
            *error = QStringLiteral("presentation access token must contain 64 hexadecimal characters");
        }
        return std::nullopt;
    }
    const QByteArray latin = value.toLatin1();
    for (const char character : latin) {
        const bool digit = character >= '0' && character <= '9';
        const bool lowerHex = character >= 'a' && character <= 'f';
        if (!digit && !lowerHex) {
            if (error != nullptr) {
                *error = QStringLiteral("presentation access token must use lowercase hexadecimal");
            }
            return std::nullopt;
        }
    }
    return PresentationAccessToken(latin);
}

QString PresentationAccessToken::toHex() const
{
    return QString::fromLatin1(m_hexValue);
}

bool PresentationAccessToken::matches(QStringView candidate) const noexcept
{
    const QByteArray supplied = candidate.toLatin1();
    quint32 difference = quint32(supplied.size() ^ m_hexValue.size());
    for (qsizetype index = 0; index < m_hexValue.size(); ++index) {
        const uchar candidateByte = index < supplied.size()
            ? uchar(supplied.at(index))
            : uchar(0);
        difference |= quint32(uchar(m_hexValue.at(index)) ^ candidateByte);
    }
    // AGENT-GUARD: every stored byte participates even when the candidate
    // length is wrong; never replace this with an early-exit string compare.
    return difference == 0;
}

} // namespace QindaQt::Services::NotificationPresentation
