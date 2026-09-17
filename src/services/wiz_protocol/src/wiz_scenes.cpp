// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_protocol/wiz_scenes.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <QtCore/QCoreApplication>

namespace QindaQt::Wiz
{
namespace
{

struct SceneEntry {
    quint16 id;
    const char *name;
    bool dynamic;
    Feature required;
};

// AGENT-NOTE: the `required` column follows the vendor's published per-product
// scene lists: a dimmable-white luminaire runs only the eight brightness-shaped
// programmes, a tunable-white one adds the fixed white points, and the rest
// need real colour channels. Feature::Dimming therefore means "every
// luminaire", not "brightness only".
constexpr SceneEntry sceneTable[] = {
    {1, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Ocean"), true, Feature::Color},
    {2, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Romance"), true, Feature::Color},
    {3, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Sunset"), true, Feature::Color},
    {4, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Party"), true, Feature::Color},
    {5, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Fireplace"), true, Feature::Color},
    {6, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Cozy"), false, Feature::ColorTemperature},
    {7, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Forest"), true, Feature::Color},
    {8, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Pastel colours"), true, Feature::Color},
    {9, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Wake-up"), true, Feature::Dimming},
    {10, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Bedtime"), true, Feature::Dimming},
    {11, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Warm white"), false, Feature::ColorTemperature},
    {12, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Daylight"), false, Feature::ColorTemperature},
    {13, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Cool white"), false, Feature::Dimming},
    {14, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Night light"), false, Feature::Dimming},
    {15, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Focus"), false, Feature::ColorTemperature},
    {16, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Relax"), false, Feature::ColorTemperature},
    {17, QT_TRANSLATE_NOOP("QindaQt::Wiz", "True colours"), false, Feature::Color},
    {18, QT_TRANSLATE_NOOP("QindaQt::Wiz", "TV time"), false, Feature::ColorTemperature},
    {19, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Plant growth"), false, Feature::Color},
    {20, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Spring"), true, Feature::Color},
    {21, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Summer"), true, Feature::Color},
    {22, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Autumn"), true, Feature::Color},
    {23, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Deep dive"), true, Feature::Color},
    {24, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Jungle"), true, Feature::Color},
    {25, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Mojito"), true, Feature::Color},
    {26, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Club"), true, Feature::Color},
    {27, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Christmas"), true, Feature::Color},
    {28, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Halloween"), true, Feature::Color},
    {29, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Candlelight"), true, Feature::Dimming},
    {30, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Golden white"), false, Feature::Dimming},
    {31, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Pulse"), true, Feature::Dimming},
    {32, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Steampunk"), true, Feature::Dimming},
    {Limits::rhythmSceneId, QT_TRANSLATE_NOOP("QindaQt::Wiz", "Rhythm"), true,
     Feature::Color},
};

[[nodiscard]] SceneDescriptor describe(const SceneEntry &entry)
{
    SceneDescriptor descriptor;
    descriptor.id = entry.id;
    descriptor.name = QCoreApplication::translate("QindaQt::Wiz", entry.name);
    descriptor.dynamic = entry.dynamic;
    descriptor.required = Features(entry.required);
    return descriptor;
}

} // namespace

QList<SceneDescriptor> allScenes()
{
    QList<SceneDescriptor> result;
    result.reserve(static_cast<qsizetype>(std::size(sceneTable)));
    for (const auto &entry : sceneTable) {
        result.append(describe(entry));
    }
    return result;
}

QList<SceneDescriptor> scenesFor(Features features)
{
    QList<SceneDescriptor> result;
    if (!features.testFlag(Feature::Scenes)) {
        return result;
    }
    for (const auto &entry : sceneTable) {
        if (features.testFlag(entry.required)) {
            result.append(describe(entry));
        }
    }
    return result;
}

std::optional<SceneDescriptor> sceneById(quint16 id)
{
    for (const auto &entry : sceneTable) {
        if (entry.id == id) {
            return describe(entry);
        }
    }
    return std::nullopt;
}

bool sceneSupported(quint16 id, Features features)
{
    if (!features.testFlag(Feature::Scenes)) {
        return false;
    }
    for (const auto &entry : sceneTable) {
        if (entry.id == id) {
            return features.testFlag(entry.required);
        }
    }
    return false;
}

} // namespace QindaQt::Wiz
