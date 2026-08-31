// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace QindaQt::Shell::TestSupport {

inline QJsonObject wireOutput(const QString &name, quint32 priority)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("geometry"),
         QJsonObject{{QStringLiteral("x"), priority == 1 ? 0 : 1920},
                     {QStringLiteral("y"), 0},
                     {QStringLiteral("width"), 1920},
                     {QStringLiteral("height"), 1080}}},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("refreshRateMilliHz"), 60'000},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("internal"), false},
        {QStringLiteral("uuid"), QStringLiteral("uuid-%1").arg(name)},
        {QStringLiteral("priority"), static_cast<qint64>(priority)},
        {QStringLiteral("physicalSizeMm"),
         QJsonObject{{QStringLiteral("width"), 0},
                     {QStringLiteral("height"), 0}}},
        {QStringLiteral("manufacturer"), QString{}},
        {QStringLiteral("model"), name},
    };
}

inline QByteArray authorityPayload(
    quint64 generation, std::initializer_list<QJsonObject> outputs)
{
    QJsonArray array;
    for (const auto &output : outputs) {
        array.append(output);
    }
    return QJsonDocument(
               QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                           {QStringLiteral("schemaVersion"), 1},
                           {QStringLiteral("outputGeneration"),
                            QString::number(generation)},
                           {QStringLiteral("outputs"), array}})
        .toJson(QJsonDocument::Compact);
}

} // namespace QindaQt::Shell::TestSupport
