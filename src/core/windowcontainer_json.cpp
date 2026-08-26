// SPDX-License-Identifier: LGPL-3.0-or-later
#include "windowcontainer.h"

#include <QJsonArray>
#include <QJsonValue>

#include <utility>

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
                                      bool allowEmpty,
                                      QString *error)
{
    const auto value = object.value(key);
    if (!value.isString() || (!allowEmpty && value.toString().isEmpty())) {
        assignError(error,
                    QStringLiteral("'%1' must be %2 string")
                        .arg(key, allowEmpty ? QStringLiteral("a")
                                             : QStringLiteral("a non-empty")));
        return std::nullopt;
    }
    return value.toString();
}

} // namespace

QJsonObject WindowContainer::toJson() const
{
    QJsonArray serializedPages;
    for (const auto &candidate : m_pages) {
        serializedPages.append(QJsonObject{{QStringLiteral("id"), candidate.id()},
                                           {QStringLiteral("root"), candidate.root().toJson()}});
    }

    return QJsonObject{{QStringLiteral("schemaVersion"), JsonSchemaVersion},
                       {QStringLiteral("id"), m_id},
                       {QStringLiteral("activePageId"), m_activePageId},
                       {QStringLiteral("pages"), serializedPages}};
}

std::optional<WindowContainer> WindowContainer::fromJson(const QJsonObject &object,
                                                         QString *error)
{
    const auto version = object.value(QStringLiteral("schemaVersion"));
    if (!version.isDouble()
        || version.toDouble() != static_cast<double>(JsonSchemaVersion)) {
        assignError(error,
                    QStringLiteral("unsupported or missing container schema version"));
        return std::nullopt;
    }

    const auto containerId = requiredString(object,
                                            QLatin1StringView("id"),
                                            false,
                                            error);
    const auto activePageId = requiredString(object,
                                             QLatin1StringView("activePageId"),
                                             true,
                                             error);
    const auto pagesValue = object.value(QStringLiteral("pages"));
    if (!containerId || !activePageId) {
        return std::nullopt;
    }
    if (!pagesValue.isArray()) {
        assignError(error, QStringLiteral("'pages' must be an array"));
        return std::nullopt;
    }

    WindowContainer parsed(*containerId);
    const auto pagesArray = pagesValue.toArray();
    for (qsizetype index = 0; index < pagesArray.size(); ++index) {
        const auto pageValue = pagesArray[index];
        if (!pageValue.isObject()) {
            assignError(error, QStringLiteral("page %1 must be an object").arg(index));
            return std::nullopt;
        }
        const auto pageObject = pageValue.toObject();
        QString pageError;
        const auto pageId = requiredString(pageObject,
                                           QLatin1StringView("id"),
                                           false,
                                           &pageError);
        const auto rootValue = pageObject.value(QStringLiteral("root"));
        if (!pageId) {
            assignError(error,
                        QStringLiteral("page %1: %2").arg(index).arg(pageError));
            return std::nullopt;
        }
        if (!rootValue.isObject()) {
            assignError(error,
                        QStringLiteral("page %1: 'root' must be an object").arg(index));
            return std::nullopt;
        }
        auto root = LayoutNode::fromJson(rootValue.toObject(), &pageError);
        if (!root) {
            assignError(error,
                        QStringLiteral("page %1 root: %2").arg(index).arg(pageError));
            return std::nullopt;
        }
        parsed.m_pages.append(ContainerPage(*pageId, std::move(*root)));
    }
    parsed.m_activePageId = *activePageId;

    // AGENT-GUARD: Node parsing validates each tree; container validation is
    // still required to catch IDs duplicated across different pages.
    const auto result = parsed.validate();
    if (!result.valid) {
        assignError(error, result.message);
        return std::nullopt;
    }
    return parsed;
}

} // namespace QindaQt::Core
