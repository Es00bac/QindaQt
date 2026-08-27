// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_protocol/settings_value_codec.h"

#include "qindaqt/services/settings_protocol/settings_wire_encode.h"
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

bool wellFormedUtf16(const QString &text)
{
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (character.isHighSurrogate()) {
            if (index + 1 >= text.size() || !text.at(index + 1).isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (character.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

} // namespace

bool BoundedSettingsValueCodec::validateKey(const QString &key, QString *error)
{
    if (key.isEmpty() || key.contains(QChar(u'\0'))) {
        setError(error, QStringLiteral("key must be non-empty and contain no NUL"));
        return false;
    }
    if (!wellFormedUtf16(key)) {
        setError(error, QStringLiteral("key contains ill-formed UTF-16"));
        return false;
    }
    if (key.toUtf8().size() > WireContract::MaximumKeyBytes) {
        setError(error, QStringLiteral("key exceeds %1 UTF-8 bytes")
                            .arg(WireContract::MaximumKeyBytes));
        return false;
    }
    return true;
}

bool BoundedSettingsValueCodec::validateString(const QString &text, QString *error)
{
    if (text.contains(QChar(u'\0'))) {
        setError(error, QStringLiteral("string contains an embedded NUL"));
        return false;
    }
    if (!wellFormedUtf16(text)) {
        setError(error, QStringLiteral("string contains ill-formed UTF-16"));
        return false;
    }
    if (text.toUtf8().size() > WireContract::MaximumStringValueBytes) {
        setError(error, QStringLiteral("string exceeds %1 UTF-8 bytes")
                            .arg(WireContract::MaximumStringValueBytes));
        return false;
    }
    return true;
}

bool BoundedSettingsValueCodec::validateValue(const QVariant &value,
                                               QString *error,
                                               Usage *usage)
{
    return encodeBoundedJsonValueForWire(value, error, usage).has_value();
}

} // namespace QindaQt::Services::SettingsProtocol
