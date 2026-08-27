// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QJsonValue>
#include <QString>
#include <QVariant>

#include <optional>

namespace QindaQt::Settings::Internal {

[[nodiscard]] bool validateCanonicalJsonText(const QString &text,
                                             QString *message = nullptr);

// AGENT-CONTRACT: Object settings use one restart-stable JSON domain. Null is
// represented in memory only by QMetaType::Nullptr; invalid QVariant is an
// absent/programming-error value and must never enter the model.
[[nodiscard]] bool normalizeCanonicalJsonValue(const QVariant &input,
                                               QVariant *normalized,
                                               QString *message);

// Encodes only the canonical domain produced above. Keeping this explicit
// avoids QVariant/QJson's lossy ULongLong and metatype-dependent conversions.
[[nodiscard]] std::optional<QJsonValue> encodeCanonicalJsonValue(
    const QVariant &value,
    QString *message = nullptr);

} // namespace QindaQt::Settings::Internal
