// SPDX-License-Identifier: GPL-3.0-or-later
#include "livecustomizationmodel.h"

#include "qindaqt/profiles/profile_types.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace QindaQt::Shell::LiveCustomizationModel {

namespace {

bool supportsAnyPanelZone(const Applets::AppletManifest &manifest)
{
    return manifest.placementZones.contains(Applets::PlacementZone::PanelStart)
        || manifest.placementZones.contains(Applets::PlacementZone::PanelCenter)
        || manifest.placementZones.contains(Applets::PlacementZone::PanelEnd);
}

bool horizontalPanel(const Profiles::PanelSpec &panel)
{
    return panel.edge == Profiles::Edge::Top || panel.edge == Profiles::Edge::Bottom;
}

QStringList zoneNames(const Applets::AppletManifest &manifest)
{
    QStringList zones;
    if (manifest.placementZones.contains(Applets::PlacementZone::PanelStart)) {
        zones.append(QStringLiteral("start"));
    }
    if (manifest.placementZones.contains(Applets::PlacementZone::PanelCenter)) {
        zones.append(QStringLiteral("center"));
    }
    if (manifest.placementZones.contains(Applets::PlacementZone::PanelEnd)) {
        zones.append(QStringLiteral("end"));
    }
    return zones;
}

// Integral, finite, int-representable bound (the Settings route's rule).
std::optional<int> integralBound(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < std::numeric_limits<int>::min()
        || number > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(number);
}

struct FieldKind final {
    QString kind; // boolean | integer | choice | empty when unsupported
    int minimum = 0;
    int maximum = 0;
    QStringList choices;
};

FieldKind classify(const QJsonObject &property)
{
    FieldKind field;
    const QString type = property.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("boolean")) {
        field.kind = QStringLiteral("boolean");
        return field;
    }
    if (type == QLatin1String("integer")) {
        const auto minimum = integralBound(property.value(QStringLiteral("minimum")));
        const auto maximum = integralBound(property.value(QStringLiteral("maximum")));
        if (!minimum || !maximum || *minimum > *maximum) {
            return {};
        }
        field.kind = QStringLiteral("integer");
        field.minimum = *minimum;
        field.maximum = *maximum;
        return field;
    }
    if (type == QLatin1String("string")) {
        const QJsonValue members = property.value(QStringLiteral("enum"));
        if (!members.isArray() || members.toArray().isEmpty()) {
            return {};
        }
        for (const QJsonValue &member : members.toArray()) {
            if (!member.isString()) {
                return {};
            }
            field.choices.append(member.toString());
        }
        field.kind = QStringLiteral("choice");
        return field;
    }
    return {};
}

bool integralVariant(const QVariant &value, qlonglong *result)
{
    switch (value.metaType().id()) {
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        *result = value.toLongLong();
        return true;
    case QMetaType::Double: {
        const double number = value.toDouble();
        if (!std::isfinite(number) || std::floor(number) != number) {
            return false;
        }
        *result = static_cast<qlonglong>(number);
        return true;
    }
    default:
        return false;
    }
}

} // namespace

QVariantList paletteRows(const QVector<Applets::AppletManifest> &manifests,
                         const Profiles::PanelSpec &panel)
{
    const auto orientation = horizontalPanel(panel) ? Applets::Orientation::Horizontal
                                                    : Applets::Orientation::Vertical;
    QVector<const Applets::AppletManifest *> admitted;
    for (const auto &manifest : manifests) {
        if (supportsAnyPanelZone(manifest) && manifest.orientations.contains(orientation)) {
            admitted.append(&manifest);
        }
    }
    std::sort(admitted.begin(), admitted.end(),
              [](const Applets::AppletManifest *left, const Applets::AppletManifest *right) {
                  const int byName = left->name.localeAwareCompare(right->name);
                  return byName != 0 ? byName < 0 : left->id < right->id;
              });
    QVariantList rows;
    for (const auto *manifest : admitted) {
        rows.append(QVariantMap{{QStringLiteral("pluginId"), manifest->id},
                                {QStringLiteral("name"), manifest->name},
                                {QStringLiteral("zones"), zoneNames(*manifest)}});
    }
    return rows;
}

QVariantList appletSettingRows(const Applets::AppletManifest &manifest,
                               const QVariantMap &storedSettings)
{
    const QJsonObject properties =
        manifest.settingsSchema.value(QStringLiteral("properties")).toObject();
    QVariantList rows;
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        if (it.key() == QLatin1String("zone")) {
            continue; // owned by the move entries, never a free setting
        }
        const QJsonObject property = it.value().toObject();
        const FieldKind field = classify(property);
        if (field.kind.isEmpty()) {
            continue;
        }
        QVariant effective = storedSettings.value(it.key());
        if (!effective.isValid()) {
            effective = property.value(QStringLiteral("default")).toVariant();
        }
        QVariantMap row{{QStringLiteral("key"), it.key()},
                        {QStringLiteral("title"),
                         property.value(QStringLiteral("title")).toString(it.key())},
                        {QStringLiteral("kind"), field.kind},
                        {QStringLiteral("value"), effective}};
        if (field.kind == QLatin1String("integer")) {
            row.insert(QStringLiteral("minimum"), field.minimum);
            row.insert(QStringLiteral("maximum"), field.maximum);
        } else if (field.kind == QLatin1String("choice")) {
            row.insert(QStringLiteral("choices"), field.choices);
        }
        rows.append(row);
    }
    return rows;
}

SettingValidation validateAppletSetting(const QJsonObject &settingsSchema,
                                        const QString &key, const QVariant &value)
{
    const QJsonObject properties = settingsSchema.value(QStringLiteral("properties")).toObject();
    if (!properties.contains(key)) {
        return {{}, QStringLiteral("'%1' is not a setting this applet declares").arg(key)};
    }
    const FieldKind field = classify(properties.value(key).toObject());
    if (field.kind == QLatin1String("boolean")) {
        if (value.metaType().id() != QMetaType::Bool) {
            return {{}, QStringLiteral("'%1' takes a switch value").arg(key)};
        }
        return {QVariant(value.toBool()), {}};
    }
    if (field.kind == QLatin1String("integer")) {
        qlonglong number = 0;
        if (!integralVariant(value, &number) || number < field.minimum || number > field.maximum) {
            return {{}, QStringLiteral("'%1' must be a whole number from %2 to %3")
                            .arg(key).arg(field.minimum).arg(field.maximum)};
        }
        return {QVariant(static_cast<int>(number)), {}};
    }
    if (field.kind == QLatin1String("choice")) {
        if (value.metaType().id() != QMetaType::QString || !field.choices.contains(value.toString())) {
            return {{}, QStringLiteral("'%1' must be one of: %2").arg(key, field.choices.join(QStringLiteral(", ")))};
        }
        return {QVariant(value.toString()), {}};
    }
    return {{}, QStringLiteral("'%1' has no typed editor").arg(key)};
}

QVariantMap panelOptions(const Profiles::PanelSpec &panel)
{
    return {{QStringLiteral("id"), panel.id},
            {QStringLiteral("output"), panel.output},
            {QStringLiteral("edge"), Profiles::toString(panel.edge)},
            {QStringLiteral("alignment"), Profiles::toString(panel.alignment)},
            {QStringLiteral("layer"), Profiles::toString(panel.layer)},
            {QStringLiteral("hideMode"), Profiles::toString(panel.hideMode)},
            {QStringLiteral("rows"), panel.rows},
            {QStringLiteral("thickness"), panel.thickness},
            {QStringLiteral("length"), panel.length},
            {QStringLiteral("appletCount"), static_cast<int>(panel.applets.size())}};
}

const Profiles::PanelSpec *findPanel(const Profiles::LayoutProfile &profile, const QString &panelId)
{
    for (const auto &panel : profile.panels) {
        if (panel.id == panelId) {
            return &panel;
        }
    }
    return nullptr;
}

const Profiles::AppletSpec *findApplet(const Profiles::PanelSpec &panel, const QString &appletId)
{
    for (const auto &applet : panel.applets) {
        if (applet.id == appletId) {
            return &applet;
        }
    }
    return nullptr;
}

const Applets::AppletManifest *findManifest(const QVector<Applets::AppletManifest> &manifests,
                                            const QString &pluginId)
{
    for (const auto &manifest : manifests) {
        if (manifest.id == pluginId) {
            return &manifest;
        }
    }
    return nullptr;
}

QString appletZone(const Profiles::AppletSpec &applet)
{
    const QVariant zone = applet.settings.value(QStringLiteral("zone"));
    return zone.isValid() ? zone.toString() : QStringLiteral("start");
}

std::optional<ZoneMove> zoneMove(const Profiles::PanelSpec &panel, const QString &appletId,
                                 const QString &zone)
{
    if (findApplet(panel, appletId) == nullptr
        || !ShellCustomizationEditor::isValidEditorZone(zone) || zone == QLatin1String("desktop")) {
        return std::nullopt;
    }
    // Simulate the flat list: take the applet out, put it back right after
    // the zone's last applet, and compare with today's order. The anchor is
    // whatever now follows that slot; nothing means append at the panel end.
    QStringList order;
    QHash<QString, QString> zones;
    for (const auto &applet : panel.applets) {
        order.append(applet.id);
        zones.insert(applet.id, appletZone(applet));
    }
    QStringList without = order;
    without.removeAll(appletId);
    qsizetype insertAt = 0;
    for (qsizetype index = 0; index < without.size(); ++index) {
        if (zones.value(without.at(index)) == zone) {
            insertAt = index + 1;
        }
    }
    QStringList result = without;
    result.insert(insertAt, appletId);
    ZoneMove move;
    move.target.panelId = panel.id;
    move.target.zone = zone;
    if (insertAt < without.size()) {
        move.target.beforeAppletId = without.at(insertAt);
    }
    move.flatOrderUnchanged = result == order;
    return move;
}

std::optional<ShellCustomizationEditor::DropTarget>
stepMove(const Profiles::PanelSpec &panel, const QString &appletId, int delta)
{
    const auto *applet = findApplet(panel, appletId);
    if (applet == nullptr || delta == 0) {
        return std::nullopt;
    }
    const QString zone = appletZone(*applet);
    QVector<qsizetype> zoneIndices;
    qsizetype selfPosition = -1;
    for (qsizetype index = 0; index < panel.applets.size(); ++index) {
        if (appletZone(panel.applets.at(index)) != zone) {
            continue;
        }
        if (panel.applets.at(index).id == appletId) {
            selfPosition = zoneIndices.size();
        }
        zoneIndices.append(index);
    }
    const qsizetype neighbour = selfPosition + (delta < 0 ? -1 : 1);
    if (selfPosition < 0 || neighbour < 0 || neighbour >= zoneIndices.size()) {
        return std::nullopt; // already at the zone edge
    }
    ShellCustomizationEditor::DropTarget target;
    target.panelId = panel.id;
    target.zone = zone;
    if (delta < 0) {
        target.beforeAppletId = panel.applets.at(zoneIndices.at(neighbour)).id;
    } else {
        // Insert before the applet after the neighbour, or append.
        const qsizetype after = zoneIndices.at(neighbour) + 1;
        if (after < panel.applets.size()) {
            target.beforeAppletId = panel.applets.at(after).id;
        }
    }
    return target;
}

std::optional<ShellCustomizationEditor::DropTarget>
panelMove(const Profiles::LayoutProfile &profile, const QString &sourcePanelId,
          const QString &appletId, const QString &targetPanelId)
{
    const auto *source = findPanel(profile, sourcePanelId);
    const auto *target = findPanel(profile, targetPanelId);
    if (source == nullptr || target == nullptr || sourcePanelId == targetPanelId) {
        return std::nullopt;
    }
    const auto *applet = findApplet(*source, appletId);
    if (applet == nullptr) {
        return std::nullopt;
    }
    ShellCustomizationEditor::DropTarget result;
    result.panelId = targetPanelId;
    result.zone = appletZone(*applet);
    return result;
}

QString nextInstanceId(const Profiles::LayoutProfile &profile, const QString &pluginId)
{
    QSet<QString> used;
    for (const auto &panel : profile.panels) {
        for (const auto &applet : panel.applets) {
            used.insert(applet.id);
        }
    }
    for (const auto &applet : profile.desktopApplets) {
        used.insert(applet.id);
    }
    for (int suffix = 1; suffix <= 10'000; ++suffix) {
        const QString candidate = pluginId + QStringLiteral("-instance-") + QString::number(suffix);
        if (!used.contains(candidate)) {
            return candidate;
        }
    }
    return {};
}

QString nextPanelId(const Profiles::LayoutProfile &profile)
{
    for (int suffix = 1; suffix <= 10'000; ++suffix) {
        const QString candidate = QStringLiteral("panel-") + QString::number(suffix);
        if (findPanel(profile, candidate) == nullptr) {
            return candidate;
        }
    }
    return {};
}

Profiles::PanelSpec defaultPanel(const QString &id, Profiles::Edge edge)
{
    Profiles::PanelSpec panel;
    panel.id = id;
    panel.output = QStringLiteral("*");
    panel.edge = edge;
    panel.layer = Profiles::Layer::Above;
    panel.hideMode = Profiles::HideMode::Never;
    panel.alignment = Profiles::Alignment::Fill;
    panel.rows = 1;
    panel.thickness = 32;
    panel.length = 1.0;
    return panel;
}

QString zoneAtFraction(double fraction)
{
    if (fraction < 1.0 / 3.0) {
        return QStringLiteral("start");
    }
    if (fraction < 2.0 / 3.0) {
        return QStringLiteral("center");
    }
    return QStringLiteral("end");
}

} // namespace QindaQt::Shell::LiveCustomizationModel
