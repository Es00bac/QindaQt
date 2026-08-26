// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/settings/settings_transaction.h"

#include <utility>

namespace QindaQt::Settings {

SettingsTransaction::SettingsTransaction(SettingLayer layer, quint64 baseRevision)
    : m_layer(layer)
    , m_baseRevision(baseRevision)
{
}

void SettingsTransaction::setValue(QString key, QVariant value)
{
    m_operations.insert(std::move(key), {SettingOperationKind::Set, std::move(value)});
}

void SettingsTransaction::removeValue(QString key)
{
    m_operations.insert(std::move(key), {SettingOperationKind::Remove, {}});
}

} // namespace QindaQt::Settings
