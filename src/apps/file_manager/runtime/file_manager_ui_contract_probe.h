// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QObject;

namespace QindaQt::Apps::FileManager {

// The --check-ui-contract probe: returns the first required object name the
// constructed QML root lacks, or an empty string when the installed package's
// whole UI surface is present. Moved out of main.cpp (ADR-0269) to keep the
// composition root within its size budget; the list itself is unchanged
// apart from the right-click set's own dialogs.
[[nodiscard]] QString missingUiContractObject(QObject *root);

} // namespace QindaQt::Apps::FileManager
