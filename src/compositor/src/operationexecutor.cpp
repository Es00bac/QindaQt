// SPDX-License-Identifier: LGPL-3.0-or-later
#include "operationexecutor.h"

#include "qindaqt/compositor/controllimits.h"

#include "windowcontainer.h"

#include <QJsonValue>
#include <QSet>

#include <cmath>
#include <limits>

namespace QindaQt::Compositor {
namespace {

bool fail(QString *code, QString *error, QString failureCode, QString message)
{
    if (code) {
        *code = std::move(failureCode);
    }
    if (error) {
        *error = std::move(message);
    }
    return false;
}

std::optional<QString> stringField(const QJsonObject &object,
                                   QLatin1StringView key,
                                   QString *code,
                                   QString *error)
{
    const auto value = object.value(key);
    if (!value.isString() || value.toString().isEmpty()) {
        fail(code, error, QStringLiteral("malformed-operation"),
             QStringLiteral("%1 must be a non-empty string").arg(key));
        return std::nullopt;
    }
    if (value.toString().size() > ControlLimits::MaxIdentifierCharacters) {
        fail(code, error, QStringLiteral("request-too-large"),
             QStringLiteral("%1 exceeds the %2-character limit")
                 .arg(key)
                 .arg(ControlLimits::MaxIdentifierCharacters));
        return std::nullopt;
    }
    return value.toString();
}

bool rejectUnknownFields(const QJsonObject &object,
                         const QSet<QString> &allowed,
                         QString *code,
                         QString *error)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!allowed.contains(iterator.key())) {
            return fail(code, error, QStringLiteral("malformed-operation"),
                        QStringLiteral("unknown field '%1'").arg(iterator.key()));
        }
    }
    return true;
}

bool mutationResult(bool result, const QString &modelError, QString *code, QString *error)
{
    return result ? true
                  : fail(code, error, QStringLiteral("mutation-rejected"), modelError);
}

bool addPage(Core::WindowContainer &container,
             const QJsonObject &operation,
             QString *code,
             QString *error)
{
    if (!rejectUnknownFields(operation,
                             {QStringLiteral("type"), QStringLiteral("pageId"),
                              QStringLiteral("leafNodeId"), QStringLiteral("windowId")},
                             code, error)) {
        return false;
    }
    const auto pageId = stringField(operation, QLatin1StringView("pageId"), code, error);
    const auto leafId = stringField(operation, QLatin1StringView("leafNodeId"), code, error);
    const auto windowId = stringField(operation, QLatin1StringView("windowId"), code, error);
    if (!pageId || !leafId || !windowId) {
        return false;
    }
    QString modelError;
    return mutationResult(container.addPage(*pageId, *leafId, *windowId, &modelError),
                          modelError, code, error);
}

bool splitWindow(Core::WindowContainer &container,
                 const QJsonObject &operation,
                 QString *code,
                 QString *error)
{
    if (!rejectUnknownFields(operation,
                             {QStringLiteral("type"), QStringLiteral("targetWindowId"),
                              QStringLiteral("newWindowId"), QStringLiteral("newLeafNodeId"),
                              QStringLiteral("splitNodeId"), QStringLiteral("orientation"),
                              QStringLiteral("ratio"), QStringLiteral("position")},
                             code, error)) {
        return false;
    }
    const auto target = stringField(operation, QLatin1StringView("targetWindowId"), code, error);
    const auto window = stringField(operation, QLatin1StringView("newWindowId"), code, error);
    const auto leaf = stringField(operation, QLatin1StringView("newLeafNodeId"), code, error);
    const auto split = stringField(operation, QLatin1StringView("splitNodeId"), code, error);
    const auto orientation = stringField(operation, QLatin1StringView("orientation"), code, error);
    const auto position = stringField(operation, QLatin1StringView("position"), code, error);
    const auto ratioValue = operation.value(QStringLiteral("ratio"));
    if (!target || !window || !leaf || !split || !orientation || !position) {
        return false;
    }
    if (!ratioValue.isDouble() || !std::isfinite(ratioValue.toDouble())) {
        return fail(code, error, QStringLiteral("malformed-operation"),
                    QStringLiteral("ratio must be a finite number"));
    }
    if (*orientation != QStringLiteral("horizontal")
        && *orientation != QStringLiteral("vertical")) {
        return fail(code, error, QStringLiteral("malformed-operation"),
                    QStringLiteral("orientation must be horizontal or vertical"));
    }
    if (*position != QStringLiteral("first") && *position != QStringLiteral("second")) {
        return fail(code, error, QStringLiteral("malformed-operation"),
                    QStringLiteral("position must be first or second"));
    }
    const Core::SplitRequest request{
        *target,
        *window,
        *leaf,
        *split,
        *orientation == QStringLiteral("horizontal")
            ? Core::SplitOrientation::Horizontal
            : Core::SplitOrientation::Vertical,
        ratioValue.toDouble(),
        *position == QStringLiteral("first")
            ? Core::InsertPosition::First
            : Core::InsertPosition::Second,
    };
    QString modelError;
    return mutationResult(container.splitWindow(request, &modelError), modelError, code, error);
}

} // namespace

bool applyOperation(Core::WindowContainer &container,
                    const QJsonObject &operation,
                    QString *code,
                    QString *error)
{
    const auto type = stringField(operation, QLatin1StringView("type"), code, error);
    if (!type) {
        return false;
    }
    if (*type == QStringLiteral("add-page")) {
        return addPage(container, operation, code, error);
    }
    if (*type == QStringLiteral("split-window")) {
        return splitWindow(container, operation, code, error);
    }

    QString modelError;
    if (*type == QStringLiteral("activate-page")) {
        if (!rejectUnknownFields(operation, {QStringLiteral("type"), QStringLiteral("pageId")}, code, error)) {
            return false;
        }
        const auto pageId = stringField(operation, QLatin1StringView("pageId"), code, error);
        return pageId && mutationResult(container.activatePage(*pageId, &modelError), modelError, code, error);
    }
    if (*type == QStringLiteral("move-page")) {
        if (!rejectUnknownFields(operation,
                                 {QStringLiteral("type"), QStringLiteral("pageId"),
                                  QStringLiteral("destinationIndex")}, code, error)) {
            return false;
        }
        const auto pageId = stringField(operation, QLatin1StringView("pageId"), code, error);
        const auto value = operation.value(QStringLiteral("destinationIndex"));
        const auto index = value.toInteger(-1);
        if (!pageId || !value.isDouble() || index < 0
            || static_cast<double>(index) != value.toDouble()) {
            return pageId ? fail(code, error, QStringLiteral("malformed-operation"),
                                 QStringLiteral("destinationIndex must be a non-negative integer"))
                          : false;
        }
        return mutationResult(container.movePage(*pageId, index, &modelError), modelError, code, error);
    }
    if (*type == QStringLiteral("swap-windows")) {
        if (!rejectUnknownFields(operation,
                                 {QStringLiteral("type"), QStringLiteral("firstWindowId"),
                                  QStringLiteral("secondWindowId")}, code, error)) {
            return false;
        }
        const auto first = stringField(operation, QLatin1StringView("firstWindowId"), code, error);
        const auto second = stringField(operation, QLatin1StringView("secondWindowId"), code, error);
        return first && second
            && mutationResult(container.swapWindows(*first, *second, &modelError), modelError, code, error);
    }
    if (*type == QStringLiteral("set-split-ratio")) {
        if (!rejectUnknownFields(operation,
                                 {QStringLiteral("type"), QStringLiteral("splitNodeId"),
                                  QStringLiteral("ratio")}, code, error)) {
            return false;
        }
        const auto splitId = stringField(operation, QLatin1StringView("splitNodeId"), code, error);
        const auto value = operation.value(QStringLiteral("ratio"));
        if (!splitId || !value.isDouble()) {
            return splitId ? fail(code, error, QStringLiteral("malformed-operation"),
                                  QStringLiteral("ratio must be a number"))
                           : false;
        }
        return mutationResult(container.setSplitRatio(*splitId, value.toDouble(), &modelError),
                              modelError, code, error);
    }
    if (*type == QStringLiteral("detach-window")) {
        if (!rejectUnknownFields(operation,
                                 {QStringLiteral("type"), QStringLiteral("windowId")},
                                 code, error)) {
            return false;
        }
        const auto windowId = stringField(operation, QLatin1StringView("windowId"), code, error);
        return windowId
            && mutationResult(container.detachWindow(*windowId, &modelError).has_value(),
                              modelError, code, error);
    }
    return fail(code, error, QStringLiteral("unsupported-operation"),
                QStringLiteral("unsupported operation type '%1'").arg(*type));
}

} // namespace QindaQt::Compositor
