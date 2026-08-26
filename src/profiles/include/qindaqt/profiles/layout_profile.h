// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/profile_types.h"

#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace QindaQt::Profiles {

struct AppletSpec {
    QString id;
    QString plugin;
    QVariantMap settings;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct PanelSpec {
    QString id;
    QString output = QStringLiteral("*");
    Edge edge = Edge::Top;
    Layer layer = Layer::Above;
    HideMode hideMode = HideMode::Never;
    Alignment alignment = Alignment::Fill;
    int rows = 1;
    int thickness = 32;
    double length = 1.0;
    QVector<AppletSpec> applets;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct WorkflowSpec {
    QString overview = QStringLiteral("compact");
    QString workspacePolicy = QStringLiteral("static");
    QString launcher = QStringLiteral("shelf");
    QString menu = QStringLiteral("global");
    QString taskList = QStringLiteral("grouped");
    bool globalMenu = true;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

class LayoutProfile final {
public:
    int schemaVersion = QINDAQT_PROFILE_SCHEMA_VERSION;
    QString id;
    QString name;
    QString description;
    QString defaultTheme = QStringLiteral("qinda-dark");
    WorkflowSpec workflow;
    QVector<PanelSpec> panels;

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] QJsonObject toJson() const;
};

} // namespace QindaQt::Profiles
