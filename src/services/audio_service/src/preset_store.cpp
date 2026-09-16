// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/preset_store.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

#include <utility>

namespace QindaQt::Audio
{
namespace {

constexpr qint64 kMaxPresetBytes = 256 * 1024;
constexpr char kSuffix[] = ".json";
// The display name is kept inside the document so the slug never has to be
// reversible.
constexpr char kNameKey[] = "presetName";

} // namespace

PresetStore::PresetStore(QString directory)
    : m_directory(std::move(directory))
{
}

QString PresetStore::defaultDirectory()
{
    const QByteArray override = qgetenv("QINDAQT_AUDIO_PRESET_DIR");
    if (!override.isEmpty()) {
        return QString::fromUtf8(override);
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("qindaqt/audio-presets"));
}

QString PresetStore::slugFor(const QString &name)
{
    if (!isBoundedText(name, kMaxPresetNameUtf8Bytes)) {
        return {};
    }
    QString slug;
    bool dash = false;
    for (const QChar character : name.toLower()) {
        if (character.isLetterOrNumber() && character.unicode() < 0x80) {
            slug.append(character);
            dash = false;
        } else if (!slug.isEmpty() && !dash) {
            slug.append(QLatin1Char('-'));
            dash = true;
        }
    }
    while (slug.endsWith(QLatin1Char('-'))) {
        slug.chop(1);
    }
    return slug;
}

QString PresetStore::pathFor(const QString &name) const
{
    const QString slug = slugFor(name);
    return slug.isEmpty() ? QString{} : QDir(m_directory).filePath(slug + QLatin1String(kSuffix));
}

QStringList PresetStore::names() const
{
    QStringList names;
    const QDir dir(m_directory);
    for (const QString &entry : dir.entryList({QStringLiteral("*.json")}, QDir::Files,
                                              QDir::Name)) {
        if (names.size() >= kMaxPresets) {
            break;
        }
        QFile file(dir.filePath(entry));
        if (file.size() > kMaxPresetBytes || !file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.read(kMaxPresetBytes));
        const QString name = document.object().value(QLatin1String(kNameKey)).toString();
        if (!name.isEmpty() && isBoundedText(name, kMaxPresetNameUtf8Bytes)) {
            names.append(name);
        }
    }
    return names;
}

bool PresetStore::save(const QString &name, const ConsoleModel &model)
{
    const QString path = pathFor(name);
    if (path.isEmpty()) {
        return false;
    }
    if (!QFileInfo::exists(path) && names().size() >= kMaxPresets) {
        return false;
    }
    if (!QDir().mkpath(m_directory)) {
        return false;
    }
    QJsonObject object = model.toJson();
    object.insert(QLatin1String(kNameKey), name);
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool PresetStore::load(const QString &name, ConsoleModel &model) const
{
    const QString path = pathFor(name);
    if (path.isEmpty()) {
        return false;
    }
    QFile file(path);
    if (!file.exists() || file.size() > kMaxPresetBytes || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.read(kMaxPresetBytes + 1);
    if (bytes.size() > kMaxPresetBytes) {
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    model.loadJson(document.object());
    return true;
}

bool PresetStore::remove(const QString &name)
{
    const QString path = pathFor(name);
    return !path.isEmpty() && QFile::exists(path) && QFile::remove(path);
}

} // namespace QindaQt::Audio
