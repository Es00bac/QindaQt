// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace QindaQt::DesignTokens { class DesignTokens; }
namespace QindaQt::Apps::Terminal {
// Pure Qt Widgets chrome adapter. Scoped selectors deliberately never style
// qtermwidget internals or overwrite renderer-owned font/selection semantics.
[[nodiscard]] QString terminalChromeStyleSheet(
    const QindaQt::DesignTokens::DesignTokens &tokens);
}
