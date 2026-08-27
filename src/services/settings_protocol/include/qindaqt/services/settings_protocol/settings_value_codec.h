// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QVariant>

namespace QindaQt::Services::SettingsProtocol {

// AGENT-CONTRACT: this codec enforces only wire-level bounds. It has no knowledge of
// settings domains, types, or defaults -- that validation belongs to
// QindaQt::Settings::SettingsSchema on the service side. A value that passes
// this codec can still be rejected by the schema (wrong type for its key,
// out of range, unknown enum, ...); this codec exists so a malformed or
// hostile caller cannot force the service to process an unbounded payload
// before schema validation. Every JSON-native shape accepted by Object keys
// remains representable, including nested arrays and objects.
class BoundedSettingsValueCodec final {
public:
    struct Usage final {
        qsizetype bytes = 0;
        qsizetype nodes = 0;
        qsizetype maximumDepth = 0;
    };

    [[nodiscard]] static bool validateKey(const QString &key, QString *error = nullptr);
    [[nodiscard]] static bool validateString(const QString &text, QString *error = nullptr);

    [[nodiscard]] static bool validateValue(const QVariant &value,
                                            QString *error = nullptr,
                                            Usage *usage = nullptr);
};

} // namespace QindaQt::Services::SettingsProtocol
