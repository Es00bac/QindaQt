// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/lan_settings.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace QindaQt::Link {
namespace {
QJsonObject readObject(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}
}

QString LanSettings::path() const { return QDir(m_directory).filePath(QStringLiteral("service.json")); }

bool LanSettings::lanEnabled() const
{
    const QJsonValue v = readObject(path()).value(QStringLiteral("lanEnabled"));
    return v.isBool() && v.toBool();
}

bool LanSettings::setLanEnabled(bool enabled, QString *error) const
{
    auto fail = [error](const QString &why) {
        if (error != nullptr) *error = why;
        return false;
    };
    if (!QDir().mkpath(m_directory)) return fail(QStringLiteral("cannot create the settings directory"));
    QJsonObject o = readObject(path());
    o.insert(QStringLiteral("lanEnabled"), enabled);
    QSaveFile f(path());
    if (!f.open(QIODevice::WriteOnly)) return fail(f.errorString());
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit() ? true : fail(f.errorString());
}

} // namespace QindaQt::Link
