// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyring_reply_validation.h"
#include <QRegularExpression>
#include <stdexcept>
namespace QindaQt::Services::KeyringClient {
namespace {
void require(bool value) { if (!value) throw std::runtime_error("Invalid keyring reply"); }
QString text(const QVariant &v, int maximum) {
    require(v.metaType()==QMetaType::fromType<QString>());
    const auto s=v.toString(); require(!s.contains(QChar(0)) && s.toUtf8().size()<=maximum); return s;
}
bool boolean(const QVariant &v) {
    require(v.metaType()==QMetaType::fromType<bool>()); return v.toBool();
}
}
bool validObjectPath(const QString &path,const QString &component) {
    if(path.size()>256) return false;
    const auto prefix=QStringLiteral("/org/freedesktop/secrets/")+component+QLatin1Char('/');
    if(!path.startsWith(prefix)) return false;
    static const QRegularExpression suffix(QStringLiteral("^[a-zA-Z0-9_]+(?:/[a-zA-Z0-9_]+)*$"));
    return suffix.match(path.mid(prefix.size())).hasMatch();
}
QVariantMap validateMetadata(const QVariantMap &wire,bool collection) {
    require(wire.size()<=8);
    const auto path=qindaqt::keyring::protocol::argument<QDBusObjectPath>(wire.value("Path")).path();
    require(validObjectPath(path,"collection"));
    const bool locked=boolean(wire.value("Locked")), authenticated=boolean(wire.value("IndexAuthenticated"));
    QVariantMap row{{"path",path},{"locked",locked},{"indexAuthenticated",authenticated}};
    if(collection || !locked) row.insert("label",text(wire.value("Label"),1024));
    else require(!wire.contains("Label") && !wire.contains("CreatedBy") && !wire.contains("Created") && !wire.contains("Modified"));
    if(!collection && !locked) {
        require(authenticated);
        for(const auto &key:{QStringLiteral("Created"),QStringLiteral("Modified")}) {
            require(wire.value(key).metaType()==QMetaType::fromType<quint64>());
            row.insert(key.toLower(),wire.value(key));
        }
        if(wire.contains("CreatedBy")) row.insert("createdBy",text(wire.value("CreatedBy"),1024));
    }
    return row;
}
QVariantMap validatePolicy(const QVariantMap &wire) {
    require(wire.size()==6);
    for(const auto &key:{"SettingsAvailable","ScreenLockAvailable","IdleAvailable","ScreenLocked","LockOnScreenLock"}) boolean(wire.value(key));
    const auto idle=wire.value("LockAfterIdleMinutes");
    require(idle.metaType()==QMetaType::fromType<int>() && idle.toInt()>=0 && idle.toInt()<=1440);
    return wire;
}
QVariantList validateCollections(const QVariantMap &wire) {
    require(wire.size()<=64); QVariantList rows;
    for(auto i=wire.begin();i!=wire.end();++i) {
        require(i.key().size()<=60);
        const auto row=validateMetadata(qindaqt::keyring::protocol::argument<QVariantMap>(i.value()),true);
        require(!rows.contains(row)); rows.append(row);
    }
    return rows;
}
QVariantList validateItems(const qindaqt::keyring::protocol::MetadataRows &wire) {
    require(wire.size()<=1024); QVariantList rows; QSet<QString> paths;
    for(const auto &value:wire) {
        const auto row=validateMetadata(value,false); const auto path=row.value("path").toString();
        require(!paths.contains(path)); paths.insert(path); rows.append(row);
    }
    return rows;
}
}
