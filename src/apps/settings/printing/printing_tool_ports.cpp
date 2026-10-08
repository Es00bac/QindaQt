// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_printing/printing_tool_ports.h>

namespace QindaQt::Apps::SettingsPrinting {
QString printingToolId(const PrintingTool tool) {
    switch (tool) {
    case PrintingTool::PrintSettings: return QStringLiteral("print-settings");
    case PrintingTool::CupsAdministration: return QStringLiteral("cups-administration");
    case PrintingTool::DocumentScanner: return QStringLiteral("document-scanner");
    }
    return {};
}
std::optional<PrintingTool> printingToolForId(const QString &id) {
    for (const auto tool : {PrintingTool::PrintSettings, PrintingTool::CupsAdministration,
                           PrintingTool::DocumentScanner})
        if (printingToolId(tool) == id) return tool;
    return std::nullopt;
}
} // namespace QindaQt::Apps::SettingsPrinting
