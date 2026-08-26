// SPDX-License-Identifier: LGPL-3.0-or-later
#include "layoutnode.h"

#include <QJsonValue>

namespace QindaQt::Core {
namespace {

void assignError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

std::optional<QString> requiredString(const QJsonObject &object,
                                      QLatin1StringView key,
                                      QString *error)
{
    const auto value = object.value(key);
    if (!value.isString() || value.toString().isEmpty()) {
        assignError(error, QStringLiteral("'%1' must be a non-empty string").arg(key));
        return std::nullopt;
    }
    return value.toString();
}

} // namespace

QJsonObject LayoutNode::toJson() const
{
    QJsonObject object{{QStringLiteral("id"), m_id}};
    if (isLeaf()) {
        object.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        object.insert(QStringLiteral("windowId"), m_windowId);
        return object;
    }

    object.insert(QStringLiteral("type"), QStringLiteral("split"));
    object.insert(QStringLiteral("orientation"),
                  m_orientation == SplitOrientation::Horizontal
                      ? QStringLiteral("horizontal")
                      : QStringLiteral("vertical"));
    object.insert(QStringLiteral("ratio"), m_ratio);
    object.insert(QStringLiteral("first"), m_first->toJson());
    object.insert(QStringLiteral("second"), m_second->toJson());
    return object;
}

std::optional<LayoutNode> LayoutNode::fromJson(const QJsonObject &object, QString *error)
{
    const auto nodeId = requiredString(object, QLatin1StringView("id"), error);
    const auto type = requiredString(object, QLatin1StringView("type"), error);
    if (!nodeId || !type) {
        return std::nullopt;
    }

    std::optional<LayoutNode> parsed;
    if (*type == QStringLiteral("leaf")) {
        const auto windowId = requiredString(object, QLatin1StringView("windowId"), error);
        if (!windowId) {
            return std::nullopt;
        }
        parsed.emplace(makeLeaf(*nodeId, *windowId));
    } else if (*type == QStringLiteral("split")) {
        const auto orientationText = requiredString(object,
                                                    QLatin1StringView("orientation"),
                                                    error);
        const auto ratioValue = object.value(QStringLiteral("ratio"));
        const auto firstValue = object.value(QStringLiteral("first"));
        const auto secondValue = object.value(QStringLiteral("second"));
        if (!orientationText) {
            return std::nullopt;
        }
        if (*orientationText != QStringLiteral("horizontal")
            && *orientationText != QStringLiteral("vertical")) {
            assignError(error, QStringLiteral("'orientation' must be horizontal or vertical"));
            return std::nullopt;
        }
        if (!ratioValue.isDouble()) {
            assignError(error, QStringLiteral("'ratio' must be a number"));
            return std::nullopt;
        }
        if (!firstValue.isObject() || !secondValue.isObject()) {
            assignError(error, QStringLiteral("a split requires object-valued first and second children"));
            return std::nullopt;
        }

        QString childError;
        auto first = fromJson(firstValue.toObject(), &childError);
        if (!first) {
            assignError(error, QStringLiteral("first child: ") + childError);
            return std::nullopt;
        }
        auto second = fromJson(secondValue.toObject(), &childError);
        if (!second) {
            assignError(error, QStringLiteral("second child: ") + childError);
            return std::nullopt;
        }
        parsed.emplace(makeSplit(*nodeId,
                                 *orientationText == QStringLiteral("horizontal")
                                     ? SplitOrientation::Horizontal
                                     : SplitOrientation::Vertical,
                                 ratioValue.toDouble(),
                                 std::move(*first),
                                 std::move(*second)));
    } else {
        assignError(error, QStringLiteral("unknown layout node type '%1'").arg(*type));
        return std::nullopt;
    }

    QSet<QString> structuralIds;
    QSet<QString> windowIds;
    const auto result = parsed->validate(structuralIds, windowIds, QStringLiteral("root"));
    if (!result.valid) {
        assignError(error, result.message);
        return std::nullopt;
    }
    return parsed;
}

} // namespace QindaQt::Core
