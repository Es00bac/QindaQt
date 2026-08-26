// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applets/api_version.h"
#include "qindaqt/applets/manifest_types.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::Applets {

inline constexpr int ManifestSchemaVersion = 1;

class ManifestValidation final {
public:
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] const QStringList &errors() const;
    [[nodiscard]] QString summary() const;

    void addError(QString error);

private:
    QStringList m_errors;
};

struct AppletManifest final {
    // AGENT-CONTRACT: A validated manifest remains untrusted declarative input.
    // Applet hosts separately decide package trust, isolation, and capability grants.
    int schemaVersion = ManifestSchemaVersion;
    QString id;
    QString name;
    QString description;
    ApiVersion apiVersion = ApiVersion::current();
    EntryPoint entryPoint;
    QVector<PlacementZone> placementZones;
    QVector<Orientation> orientations;
    SizingConstraints sizing;
    QVector<Capability> capabilities;
    QJsonObject settingsSchema;

    [[nodiscard]] ManifestValidation validate() const;
    [[nodiscard]] bool supportsHost(const ApiVersion &hostVersion) const;

    bool operator==(const AppletManifest &) const = default;
};

} // namespace QindaQt::Applets
