// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_printing/printing_tool_ports.h>

namespace QindaQt::Apps::SettingsPrinting {

// Owning copies of ordered XDG data roots; no environment lookup or watcher.
// Public ApplicationCatalog owns parsing, masking, bounds and argv expansion.
// Same GUI thread as its caller. Missing/unsupported entries are values;
// filesystem/catalog errors never cause activation or a raw-command fallback.
class ApplicationPrintingToolCatalog final : public PrintingToolCatalog {
public:
    explicit ApplicationPrintingToolCatalog(QStringList applicationDataRoots);
    [[nodiscard]] PrintingToolInventory inspect() const override;
    [[nodiscard]] PreparedPrintingTool prepare(PrintingTool tool) const override;

private:
    QStringList m_dataRoots;
};

} // namespace QindaQt::Apps::SettingsPrinting
