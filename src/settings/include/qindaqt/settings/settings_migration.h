// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/settings_document.h"
#include "qindaqt/settings/settings_schema.h"

namespace QindaQt::Settings {

// AGENT-CONTRACT: migrates only schemaVersion 1 documents to the active v2
// schema. Every v1 value carries forward unchanged; the v2-only
// "services.doNotDisturb" key is intentionally left unset in the migrated
// document so it resolves through the ordinary layered-resolution default
// (false) rather than being invented by the migrator. A document that is not
// valid against v1Schema, or schema objects that are not exactly versions 1
// and 2, fail without producing a document.
class SettingsMigration final {
public:
    [[nodiscard]] static DocumentLoadResult migrateV1ToV2(const QByteArray &v1Json,
                                                           const QString &origin,
                                                           const SettingsSchema &v1Schema,
                                                           const SettingsSchema &v2Schema);
};

// AGENT-CONTRACT: this is the only place callers should read a persisted
// settings document from disk when both an active and a legacy schema
// version are supported. It never writes; a caller that wants migration to
// become durable (as the manager decision requires) must explicitly save the
// returned document back out at the active version. Sniffing schemaVersion
// happens on the raw JSON before any schema-specific validation, so an
// unsupported/corrupt version is rejected without invoking either schema's
// stricter checks.
class SettingsCompatibilityLoader final {
public:
    [[nodiscard]] static DocumentLoadResult load(const QString &path,
                                                  const SettingsSchema &activeSchema,
                                                  const SettingsSchema &legacySchema);
};

} // namespace QindaQt::Settings
