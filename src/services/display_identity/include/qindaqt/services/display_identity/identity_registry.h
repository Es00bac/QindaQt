// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_identity/identity_types.h>

#include <QtCore/QJsonObject>

namespace QindaQt::DisplayIdentity
{

enum class RegistryError {
    None,
    InvalidSchema,
    UnsupportedSchema,
    InvalidShape,
    InvalidEntry,
    TooManyEntries,
    TooManyAliases,
    DuplicateStableId,
    DuplicateAlias,
    AmbiguousAlias,
    UnknownStableId,
    InvalidAlias,
    SequenceExhausted,
};

struct RegistryEntry {
    QString stableId;
    QString alias;
    QString label;
    QString lastConnector;
    QString manufacturer;
    QString model;
    bool internal = false;
    bool ambiguous = false;
    quint64 seenSequence = 0;

    friend bool operator==(const RegistryEntry &, const RegistryEntry &) = default;
};

struct Registry {
    quint32 schemaVersion = 2;
    QList<RegistryEntry> entries;

    friend bool operator==(const Registry &, const Registry &) = default;
};

struct RegistryResult {
    Registry registry;
    RegistryError error = RegistryError::None;
    QString reasonCode;
    bool migrated = false;

    [[nodiscard]] bool succeeded() const noexcept { return error == RegistryError::None; }
};

// These pure value functions borrow their arguments for the call and return
// owned results. They are reentrant and thread-safe. Decode/encode preserve a
// caller-owned destination on failure; all failures have a stable typed error
// plus a diagnostic reason code. Schema v2 writers remain able to read and
// migrate v1, while unknown schemas fail closed.
[[nodiscard]] RegistryResult decodeRegistry(const QJsonObject &document);
[[nodiscard]] RegistryResult encodeRegistry(const Registry &registry, QJsonObject &destination);
[[nodiscard]] RegistryResult reconcileRegistry(const Registry &registry,
                                               const QList<ResolvedOutput> &connectedOutputs,
                                               quint64 seenSequence);
[[nodiscard]] RegistryResult setAlias(const Registry &registry, const QString &stableId,
                                      const QString &alias);

} // namespace QindaQt::DisplayIdentity
