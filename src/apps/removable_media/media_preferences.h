// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <QMap>

namespace QindaQt::Apps::RemovableMedia {
class MediaPreferences final {
public:
    // Empty path uses this application's user config directory. Writes are
    // atomic; failed saves do not change the confirmed in-memory preference.
    explicit MediaPreferences(QString path = {});
    [[nodiscard]] QString mode(const QString &key) const;
    [[nodiscard]] bool save(const QString &key, const QString &mode, QString *error);
private:
    QString m_path;
    QMap<QString, QString> m_modes;
};
} // namespace QindaQt::Apps::RemovableMedia
