// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QString>

namespace QindaQt::DisplayService::Private
{

inline bool validUniqueBusOwner(const QString &owner)
{
    // D-Bus unique names are colon-prefixed, dot-separated ASCII elements.
    // Digits and '-' may lead an element, unlike well-known bus names.
    if (owner.size() < 4 || owner.size() > 255 || owner.front() != u':') {
        return false;
    }
    bool sawDot = false;
    bool elementHasCharacter = false;
    for (qsizetype index = 1; index < owner.size(); ++index) {
        const QChar character = owner.at(index);
        if (character == u'.') {
            if (!elementHasCharacter) {
                return false;
            }
            sawDot = true;
            elementHasCharacter = false;
            continue;
        }
        const ushort value = character.unicode();
        const bool allowed = (value >= 'A' && value <= 'Z')
            || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9')
            || value == '_' || value == '-';
        if (!allowed) {
            return false;
        }
        elementHasCharacter = true;
    }
    return sawDot && elementHasCharacter;
}

} // namespace QindaQt::DisplayService::Private
