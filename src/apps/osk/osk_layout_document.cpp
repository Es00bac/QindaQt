// SPDX-License-Identifier: GPL-3.0-or-later
#include "osk_layout_document.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace QindaQt::Apps::Osk {
namespace {

struct SpecialKey {
    KeyKind kind;
    qreal width;
};

const QHash<QString, SpecialKey> &specialKeys()
{
    static const QHash<QString, SpecialKey> keys{
        {QStringLiteral("{shift}"), {KeyKind::Shift, 1.5}},
        {QStringLiteral("{backspace}"), {KeyKind::Backspace, 1.5}},
        {QStringLiteral("{symbols}"), {KeyKind::Symbols, 1.5}},
        {QStringLiteral("{letters}"), {KeyKind::Letters, 1.5}},
        {QStringLiteral("{space}"), {KeyKind::Space, 5.0}},
        {QStringLiteral("{enter}"), {KeyKind::Enter, 1.5}},
        {QStringLiteral("{hide}"), {KeyKind::Hide, 1.0}},
        {QStringLiteral("{layout}"), {KeyKind::Layout, 1.0}},
    };
    return keys;
}

bool parseKey(const QJsonValue &value, KeyDefinition *key, QString *error)
{
    if (value.isString()) {
        const QString text = value.toString();
        if (const auto special = specialKeys().find(text); special != specialKeys().end()) {
            key->kind = special->kind;
            key->width = special->width;
            return true;
        }
        if (text.isEmpty()) {
            *error = QStringLiteral("a text key must not be empty");
            return false;
        }
        key->kind = KeyKind::Text;
        key->text = text;
        key->shiftedText = text.toUpper();
        return true;
    }
    if (!value.isObject()) {
        *error = QStringLiteral("a key must be a string or an object");
        return false;
    }
    const QJsonObject object = value.toObject();
    const QString text = object.value(QLatin1String("text")).toString();
    if (text.isEmpty()) {
        *error = QStringLiteral("an object key needs a non-empty \"text\"");
        return false;
    }
    key->kind = KeyKind::Text;
    key->text = text;
    key->shiftedText = object.value(QLatin1String("shift")).toString(text.toUpper());
    key->width = object.value(QLatin1String("width")).toDouble(1.0);
    if (key->width <= 0.0) {
        *error = QStringLiteral("key \"%1\" has a non-positive width").arg(text);
        return false;
    }
    return true;
}

bool parsePage(const QJsonValue &value, QList<KeyRow> *page, const QString &pageName, QString *error)
{
    if (!value.isArray()) {
        *error = QStringLiteral("\"%1\" must be an array of rows").arg(pageName);
        return false;
    }
    const QJsonArray rows = value.toArray();
    for (const QJsonValue &rowValue : rows) {
        if (!rowValue.isArray()) {
            *error = QStringLiteral("\"%1\" rows must be arrays").arg(pageName);
            return false;
        }
        KeyRow row;
        const QJsonArray keys = rowValue.toArray();
        for (const QJsonValue &keyValue : keys) {
            KeyDefinition key;
            if (!parseKey(keyValue, &key, error)) {
                return false;
            }
            row.append(key);
        }
        if (row.isEmpty()) {
            *error = QStringLiteral("\"%1\" has an empty row").arg(pageName);
            return false;
        }
        page->append(row);
    }
    return true;
}

} // namespace

QString keyKindName(KeyKind kind)
{
    switch (kind) {
    case KeyKind::Text: return QStringLiteral("text");
    case KeyKind::Shift: return QStringLiteral("shift");
    case KeyKind::Backspace: return QStringLiteral("backspace");
    case KeyKind::Symbols: return QStringLiteral("symbols");
    case KeyKind::Letters: return QStringLiteral("letters");
    case KeyKind::Space: return QStringLiteral("space");
    case KeyKind::Enter: return QStringLiteral("enter");
    case KeyKind::Hide: return QStringLiteral("hide");
    case KeyKind::Layout: return QStringLiteral("layout");
    }
    return QStringLiteral("text");
}

OskLayoutDocument OskLayoutDocument::fromJson(const QByteArray &json, QString *error)
{
    QString localError;
    QString &diagnostic = error != nullptr ? *error : localError;
    diagnostic.clear();
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        diagnostic = parseError.error != QJsonParseError::NoError
            ? parseError.errorString()
            : QStringLiteral("a layout document must be a JSON object");
        return {};
    }
    const QJsonObject object = document.object();
    OskLayoutDocument result;
    result.name = object.value(QLatin1String("name")).toString().trimmed().toLower();
    if (result.name.isEmpty()) {
        diagnostic = QStringLiteral("a layout document needs a \"name\"");
        return {};
    }
    result.label = object.value(QLatin1String("label")).toString(result.name.toUpper());
    if (!parsePage(object.value(QLatin1String("letters")), &result.letters, QStringLiteral("letters"), &diagnostic)) {
        return {};
    }
    if (object.contains(QLatin1String("symbols"))
        && !parsePage(object.value(QLatin1String("symbols")), &result.symbols, QStringLiteral("symbols"), &diagnostic)) {
        return {};
    }
    return result;
}

OskLayoutDocument OskLayoutDocument::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("cannot read %1: %2").arg(path, file.errorString());
        }
        return {};
    }
    return fromJson(file.readAll(), error);
}

} // namespace QindaQt::Apps::Osk
