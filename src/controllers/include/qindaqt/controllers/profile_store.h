// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controller_policy.h"
#include <QJsonArray>

namespace QindaQt::Controllers {
// GUI-thread confined. Owns one atomic user-config file; never stores secrets.
class ProfileStore {
public:
    explicit ProfileStore(QString path);
    bool load(QString &reason);
    bool save(QString &reason) const;
    Profile profile(const QString &id, const QString &family = "generic") const;
    void set(const QString &id, const Profile &profile, const QJsonObject &description = {});
    QJsonObject description(const QString &id) const;
    QStringList ids() const;
private:
    QString m_path;
    QMap<QString, Profile> m_profiles;
    QMap<QString, QJsonObject> m_descriptions;
};
} // namespace QindaQt::Controllers
