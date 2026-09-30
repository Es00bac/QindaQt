// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/window_management/command.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <array>
#include <cmath>
namespace QindaQt::WindowManagement {
namespace {
constexpr std::array OperationNames{
    "focus", "raise", "minimize", "restore", "close", "shade", "unshade",
    "iconify", "uniconify", "maximize", "fullscreen", "rename", "color",
    "place", "detach", "group-tab", "group-tile", "next-tab", "previous-tab",
    "activate-tab", "reorder-tab", "resize-split", "launch"};
constexpr std::array StatusNames{
    "accepted", "dispatched", "ambiguous", "stale", "invalid", "denied",
    "unavailable", "cancelled", "resource-limit"};
bool fields(const QJsonObject &object, const QStringList &allowed)
{
    for (auto i = object.begin(); i != object.end(); ++i)
        if (!allowed.contains(i.key())) return false;
    return true;
}
bool text(const QJsonValue &value, qsizetype maximum, bool empty = false)
{
    if (!value.isString()) return false;
    const QString s = value.toString();
    if ((!empty && s.trimmed().isEmpty()) || s.size() > maximum || !s.isValidUtf16()) return false;
    for (const auto c : s)
        if (c.category() == QChar::Other_Control)
            return false;
    return true;
}
std::optional<Target> target(const QJsonValue &value)
{
    if (!value.isObject()) return std::nullopt;
    const auto o = value.toObject();
    if (!fields(o, {"kind", "id", "name"}) || !o.value("kind").isString()) return std::nullopt;
    const auto kind = o.value("kind").toString();
    Target result;
    if (kind == "current") return o.size() == 1 ? std::optional(result) : std::nullopt;
    if (kind == "window") result.kind = Target::Kind::Window;
    else if (kind == "container") result.kind = Target::Kind::Container;
    else return std::nullopt;
    if (o.size() != 2 || (o.contains("id") == o.contains("name"))) return std::nullopt;
    const auto key = o.contains("id") ? QStringLiteral("id") : QStringLiteral("name");
    if (!text(o.value(key), key == "id" ? 128 : 256)) return std::nullopt;
    if (key == "id") result.id = o.value(key).toString();
    else result.name = o.value(key).toString().trimmed();
    return result;
}
bool number(const QJsonValue &value, double low, double high, bool integer = false)
{
    if (!value.isDouble()) return false;
    const double n = value.toDouble();
    return std::isfinite(n) && n >= low && n <= high && (!integer || std::floor(n) == n);
}
bool region(const QJsonValue &value)
{
    if (!value.isArray() || value.toArray().size() != 4) return false;
    const auto a = value.toArray();
    for (const auto &n : a) if (!number(n, 0, 1)) return false;
    return a[2].toDouble() > 0 && a[3].toDouble() > 0
        && a[0].toDouble() + a[2].toDouble() <= 1
        && a[1].toDouble() + a[3].toDouble() <= 1;
}
bool direction(const QJsonValue &value)
{
    return value.isString() && QStringList{"right", "down", "left", "up"}.contains(value.toString());
}
bool arguments(Operation op, const QJsonObject &a)
{
    switch (op) {
    case Operation::Maximize:
        return fields(a, {"fraction"}) && (!a.contains("fraction") || number(a.value("fraction"), .1, 1));
    case Operation::Fullscreen:
        return a.size() == 1 && a.value("enabled").isBool();
    case Operation::Rename:
        return a.size() == 1 && text(a.value("name"), 128);
    case Operation::Color:
        return a.size() == 1 && a.value("value").isString()
            && QRegularExpression(QStringLiteral("^#[0-9a-fA-F]{6}$")).match(a.value("value").toString()).hasMatch();
    case Operation::Place:
        return a.size() == 1 && region(a.value("region"));
    case Operation::GroupTab:
        return a.size() == 1 && target(a.value("destination")).has_value();
    case Operation::GroupTile:
        return fields(a, {"destination", "direction", "ratio"})
            && target(a.value("destination")).has_value() && direction(a.value("direction"))
            && (!a.contains("ratio") || number(a.value("ratio"), .05, .95));
    case Operation::ActivateTab:
    case Operation::ReorderTab:
        return a.size() == 1 && number(a.value("index"), 1, 256, true);
    case Operation::ResizeSplit:
        return a.size() == 1 && number(a.value("ratio"), .05, .95);
    case Operation::Launch: {
        if (!fields(a, {"desktopEntryId", "placement", "destination", "region", "containerName"})
            || !text(a.value("desktopEntryId"), 256) || !a.value("placement").isString()) return false;
        const auto id = a.value("desktopEntryId").toString();
        if (id.contains('/') || !id.endsWith(".desktop")) return false;
        const auto p = a.value("placement").toString();
        if (p != "tab" && p != "independent" && !direction(a.value("placement"))) return false;
        return (!a.contains("destination") || target(a.value("destination")).has_value())
            && (!a.contains("region") || region(a.value("region")))
            && (!a.contains("containerName") || text(a.value("containerName"), 128));
    }
    default: return a.isEmpty();
    }
}
} // namespace
QString operationName(Operation operation)
{
    const auto i = static_cast<size_t>(operation);
    return i < OperationNames.size() ? QString::fromLatin1(OperationNames[i]) : QString{};
}
QString statusName(Status status)
{
    const auto i = static_cast<size_t>(status);
    return i < StatusNames.size() ? QString::fromLatin1(StatusNames[i]) : QStringLiteral("invalid");
}
std::optional<Command> decodeCommand(const QByteArray &wire, QString *error)
{
    const auto fail = [error](QString message) -> std::optional<Command> {
        if (error) *error = std::move(message);
        return std::nullopt;
    };
    if (wire.isEmpty() || wire.size() > MaximumRequestBytes) return fail("request size is invalid");
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(wire, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) return fail("request must be a JSON object");
    const auto object = doc.object();
    if (object.size() != 4 || !fields(object, {"version", "operation", "target", "arguments"})
        || !number(object.value("version"), 1, 1, true) || !object.value("operation").isString()
        || !object.value("arguments").isObject()) return fail("request version or fields are invalid");
    const auto op = object.value("operation").toString();
    std::optional<Operation> parsed;
    for (size_t i = 0; i < OperationNames.size(); ++i)
        if (op == QLatin1StringView(OperationNames[i])) parsed = static_cast<Operation>(i);
    const auto selected = target(object.value("target"));
    if (!parsed || !selected) return fail("operation or target is invalid");
    const auto args = object.value("arguments").toObject();
    if (!arguments(*parsed, args)) return fail("operation arguments are invalid");
    return Command{*parsed, *selected, args};
}
QByteArray encodeResult(const Result &result)
{
    QJsonObject value{{"version", 1}, {"status", statusName(result.status)}, {"message", result.message.left(512)}};
    if (!result.contextId.isEmpty()) value.insert("contextId", result.contextId);
    if (!result.windowId.isEmpty()) value.insert("windowId", result.windowId);
    if (!result.containerId.isEmpty()) value.insert("containerId", result.containerId);
    if (!result.candidates.isEmpty()) {
        QJsonArray names;
        for (const auto &name : result.candidates.mid(0, 8)) names.append(name.left(256));
        value.insert("candidates", names);
    }
    return QJsonDocument(value).toJson(QJsonDocument::Compact);
}
} // namespace QindaQt::WindowManagement
