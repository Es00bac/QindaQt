// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QObject>
#include <QStringList>

namespace QindaQt::Apps::SettingsCustomize {

struct CustomizeOutputSnapshot final {
    QVector<ShellLayout::LogicalOutput> outputs;
    QStringList primaryOutputIds;
    quint64 revision = 0;
    QString error;
};

struct PrimaryOutputResolution final {
    QString outputId;
    QString error;

    [[nodiscard]] bool ok() const noexcept { return error.isEmpty(); }
};

// Validates that exactly one injected primary identity names exactly one member
// of the same output inventory. The exact identifier is returned unchanged.
[[nodiscard]] PrimaryOutputResolution
resolvePrimaryOutput(const CustomizeOutputSnapshot &snapshot);

// Customize owns this narrow GUI-thread inventory seam. Implementations retain
// their platform objects; callers receive value snapshots and must treat a
// revision change as invalidating an in-progress edit.
class CustomizeOutputProvider : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~CustomizeOutputProvider() override = default;

    [[nodiscard]] virtual CustomizeOutputSnapshot snapshot() const = 0;

Q_SIGNALS:
    void snapshotChanged();
};

} // namespace QindaQt::Apps::SettingsCustomize
