// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "native_voice_wire_p.h"
#include <QtCore/QVariantMap>
namespace QindaQt::Services::VoiceConfiguration {
bool nativeMap(DBusMessage *message, QVariantMap *value);
bool appendArguments(DBusMessage *message, const QVariantList &arguments);
}
