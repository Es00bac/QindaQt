// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_printing/application_printing_tool_catalog.h>
#include <qindaqt/application_catalog/application_directory_scan.h>
#include <qindaqt/application_catalog/launch_support.h>
#include <utility>

namespace QindaQt::Apps::SettingsPrinting {
namespace {
using QindaQt::ApplicationCatalog::DirectoryScan;
using QindaQt::ApplicationCatalog::ScannedApplication;

const ScannedApplication *application(const DirectoryScan &scan, const PrintingTool tool) {
    // AGENT-CONTRACT: these are installed desktop identities, not executable
    // names or user configuration. Gentoo's vendor prefix has a portable
    // unprefixed counterpart; an installed but unsupported first choice never
    // falls through to bypass its policy. ADR-0356.
    switch (tool) {
    case PrintingTool::PrintSettings:
        if (const auto *entry = scan.application(QStringLiteral("Gentoo-system-config-printer")))
            return entry;
        return scan.application(QStringLiteral("system-config-printer"));
    case PrintingTool::CupsAdministration: return scan.application(QStringLiteral("cups"));
    case PrintingTool::DocumentScanner: return scan.application(QStringLiteral("org.gnome.SimpleScan"));
    }
    return nullptr;
}
PreparedPrintingTool prepareFromScan(const DirectoryScan &scan, const PrintingTool tool) {
    const auto *entry = application(scan, tool);
    if (entry == nullptr) return {};
    const auto plan = QindaQt::ApplicationCatalog::planApplicationLaunch(
        entry->documentText, {}, entry->entry.name, entry->desktopFilePath);
    if (plan.support != QindaQt::ApplicationCatalog::LaunchSupport::ProcessSpawn)
        return {{PrintingToolAvailability::Unsupported, entry->entry.name}, {}, {}};
    return {{PrintingToolAvailability::Available, entry->entry.name},
            plan.program, plan.arguments};
}
}
ApplicationPrintingToolCatalog::ApplicationPrintingToolCatalog(QStringList roots)
    : m_dataRoots(std::move(roots)) {}
PrintingToolInventory ApplicationPrintingToolCatalog::inspect() const {
    const auto scan = QindaQt::ApplicationCatalog::scanApplicationDirectories(m_dataRoots);
    PrintingToolInventory result;
    for (std::size_t index = 0; index < PrintingToolCount; ++index)
        result[index] = prepareFromScan(scan, static_cast<PrintingTool>(index)).state;
    return result;
}
PreparedPrintingTool ApplicationPrintingToolCatalog::prepare(const PrintingTool tool) const {
    // Inventory does not grant future launch authority. Read the exact public
    // catalog bytes again when the user clicks, including higher-root masking.
    return prepareFromScan(QindaQt::ApplicationCatalog::scanApplicationDirectories(m_dataRoots), tool);
}
} // namespace QindaQt::Apps::SettingsPrinting
