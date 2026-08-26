// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QindaQt::Profiles::TestFixtures {

inline QJsonObject validProfileObject()
{
    return {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("id"), QStringLiteral("fixture")},
        {QStringLiteral("name"), QStringLiteral("Fixture")},
        {QStringLiteral("panels"),
         QJsonArray{QJsonObject{
             {QStringLiteral("id"), QStringLiteral("main")},
             {QStringLiteral("output"), QStringLiteral("*")},
             {QStringLiteral("edge"), QStringLiteral("top")},
             {QStringLiteral("layer"), QStringLiteral("above")},
             {QStringLiteral("hideMode"), QStringLiteral("never")},
             {QStringLiteral("alignment"), QStringLiteral("fill")},
             {QStringLiteral("rows"), 1},
             {QStringLiteral("thickness"), 32},
             {QStringLiteral("length"), 1.0},
             {QStringLiteral("applets"),
              QJsonArray{QJsonObject{
                  {QStringLiteral("id"), QStringLiteral("clock-instance")},
                  {QStringLiteral("plugin"), QStringLiteral("clock")},
                  {QStringLiteral("settings"),
                   QJsonObject{{QStringLiteral("zone"), QStringLiteral("end")},
                               {QStringLiteral("seconds"), false}}},
              }}},
         }}},
    };
}

inline QByteArray encode(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

inline LayoutProfile validProfile()
{
    LayoutProfile profile;
    profile.id = QStringLiteral("fixture");
    profile.name = QStringLiteral("Fixture");

    PanelSpec panel;
    panel.id = QStringLiteral("main");
    panel.applets = {{.id = QStringLiteral("clock-instance"),
                      .plugin = QStringLiteral("clock"),
                      .settings = {{QStringLiteral("zone"), QStringLiteral("end")}}}};
    profile.panels = {panel};
    return profile;
}

} // namespace QindaQt::Profiles::TestFixtures
