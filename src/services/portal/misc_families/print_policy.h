// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "misc_policy.h"
namespace QindaQt::Services::Portal {
bool validPrintMaps(const QVariantMap &, const QVariantMap &);
bool validPrintConfiguration(const QJsonObject &);
}
