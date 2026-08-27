// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_value_codec.h"

#include "qindaqt/services/settings_protocol/settings_wire_decode.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"

#include <utility>

namespace QindaQt::Services::SettingsProtocol {
namespace {

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

bool BoundedSettingsValueCodec::validateKey(const QString &key, QString *error)
{
    if (key.isEmpty() || key.contains(QChar(u'\0'))) {
        setError(error, QStringLiteral("key must be non-empty and contain no NUL"));
        return false;
    }
    if (key.toUtf8().size() > WireContract::MaximumKeyBytes) {
        setError(error, QStringLiteral("key exceeds %1 UTF-8 bytes")
                            .arg(WireContract::MaximumKeyBytes));
        return false;
    }
    return true;
}

bool BoundedSettingsValueCodec::validateValue(const QVariant &value,
                                               QString *error,
                                               Usage *usage)
{
    return decodeBoundedJsonValue(value, error, usage).has_value();
}

} // namespace QindaQt::Services::SettingsProtocol
