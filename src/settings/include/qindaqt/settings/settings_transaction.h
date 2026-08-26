// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/settings_types.h"

#include <QMap>
#include <QString>
#include <QVariant>

namespace QindaQt::Settings {

enum class SettingOperationKind {
    Set,
    Remove,
};

struct SettingOperation final {
    SettingOperationKind kind = SettingOperationKind::Set;
    QVariant value;
};

class LayeredSettings;

class SettingsTransaction final {
public:
    [[nodiscard]] SettingLayer layer() const noexcept { return m_layer; }
    [[nodiscard]] quint64 baseRevision() const noexcept { return m_baseRevision; }
    [[nodiscard]] bool isEmpty() const noexcept { return m_operations.isEmpty(); }
    [[nodiscard]] const QMap<QString, SettingOperation> &operations() const noexcept
    {
        return m_operations;
    }

    void setValue(QString key, QVariant value);
    void removeValue(QString key);

private:
    SettingsTransaction(SettingLayer layer, quint64 baseRevision);

    SettingLayer m_layer;
    quint64 m_baseRevision;
    QMap<QString, SettingOperation> m_operations;

    friend class LayeredSettings;
};

} // namespace QindaQt::Settings
