// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringView>

#include <optional>

namespace QindaQt::Services::NotificationPresentation {

// AGENT-CONTRACT: the supervisor passes the same generated value to exactly
// one host and shell client without persistence, argv, environment, or logging.
class PresentationAccessToken final {
public:
    [[nodiscard]] static PresentationAccessToken generate();
    [[nodiscard]] static std::optional<PresentationAccessToken> fromHex(
        QStringView value,
        QString *error = nullptr);

    [[nodiscard]] QString toHex() const;
    [[nodiscard]] bool matches(QStringView candidate) const noexcept;

private:
    explicit PresentationAccessToken(QByteArray hexValue);

    QByteArray m_hexValue;
};

} // namespace QindaQt::Services::NotificationPresentation
