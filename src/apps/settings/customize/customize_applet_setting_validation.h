// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariant>

namespace QindaQt::Apps::SettingsCustomize {

// The manifest kinds this route can render a real typed editor for. Any other
// JSON-schema `type`, or a "string" property without a closed `enum`, is
// Unsupported and stays a read-only value: this route invents no free-text or
// open-ended editor (customize-settings.md).
enum class AppletSettingFieldKind {
    Boolean,
    BoundedInteger,
    EnumChoice,
    Unsupported,
};

// Classifies one manifest `settingsSchema.properties.<key>` entry. A JSON
// Schema "integer" is editable only when `minimum` and `maximum` are both
// finite, integral, int-representable numbers with minimum <= maximum (the
// dispatch's "bounded integer"); a "string" is editable only with a
// non-empty `enum` whose every member is itself a JSON string (the
// dispatch's "closed enum/string-choice"). A null, missing, non-numeric,
// fractional, out-of-`int`-width, or reversed bound, a non-string enum
// member, or any other declared kind -- a bare "string" with no enum,
// "number", "object", "array", or an absent/unknown type -- is Unsupported.
// This is a hostile-schema boundary, not just a hostile-value one: an
// accepted manifest is never re-validated member-by-member upstream.
[[nodiscard]] AppletSettingFieldKind appletSettingFieldKind(
    const QJsonObject &propertySchema);

struct AppletSettingValidation final {
    // The exact typed value to store in the applet's settings map. Only
    // meaningful when ok().
    QVariant value;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

// Validates `value` for `key` against `settingsSchema` (a manifest's raw
// `AppletManifest::settingsSchema`, i.e. `{"type":"object","properties":{...}}`).
// Rejects an unknown key, an Unsupported kind, a value of the wrong QVariant
// type for its declared kind, an out-of-[minimum,maximum] integer, or a
// string outside its declared enum. Never partially accepts a hostile
// payload: the result is either the one coerced value to store or an error,
// atomically.
[[nodiscard]] AppletSettingValidation validateAppletSettingValue(
    const QJsonObject &settingsSchema, const QString &key, const QVariant &value);

// The manifest-declared `default` for `key`, or an invalid QVariant if `key`
// is undeclared or declares none. An applet's settings map omits a field
// until it is first explicitly set, so this is the value that field is
// effectively showing until then.
[[nodiscard]] QVariant appletSettingSchemaDefault(const QJsonObject &settingsSchema,
                                                  const QString &key);

} // namespace QindaQt::Apps::SettingsCustomize
