// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QString>
#include <QVector>

class QScreen;

namespace QindaQt::ShellSurface {

enum class QtOutputInventoryErrorCode {
    None,
    MissingGuiApplication,
    WrongThread,
    EmptyInventory,
    InvalidScreenId,
    DuplicateScreenId,
    InvalidGeometry,
    InvalidScale,
    MissingScreen,
};

struct QtOutputInventoryResult {
    QVector<ShellLayout::LogicalOutput> outputs;
    QtOutputInventoryErrorCode code = QtOutputInventoryErrorCode::None;
    QString error;

    [[nodiscard]] bool ok() const noexcept
    {
        return code == QtOutputInventoryErrorCode::None;
    }
};

class QtOutputInventory final {
public:
    // These calls are GUI-thread-only. IDs are the exact QScreen names used by
    // Qt's Wayland client integration; the returned QScreen pointer is borrowed
    // and valid only until Qt reports output removal.
    [[nodiscard]] static QtOutputInventoryResult read();
    [[nodiscard]] static QScreen *screenForId(const QString &id,
                                              QString *error = nullptr);
};

} // namespace QindaQt::ShellSurface
