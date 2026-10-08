// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_printing/printing_settings_model.h>
#include <QThread>

namespace QindaQt::Apps::SettingsPrinting {
namespace {
QString title(const PrintingTool tool) {
    switch (tool) {
    case PrintingTool::PrintSettings: return PrintingSettingsModel::tr("Printers and jobs");
    case PrintingTool::CupsAdministration: return PrintingSettingsModel::tr("CUPS administration");
    case PrintingTool::DocumentScanner: return PrintingSettingsModel::tr("Document scanning");
    }
    return {};
}
QString actionText(const PrintingTool tool) {
    switch (tool) {
    case PrintingTool::PrintSettings: return PrintingSettingsModel::tr("Open Print Settings");
    case PrintingTool::CupsAdministration: return PrintingSettingsModel::tr("Open CUPS Administration");
    case PrintingTool::DocumentScanner: return PrintingSettingsModel::tr("Open Document Scanner");
    }
    return {};
}
QString description(const PrintingTool tool) {
    switch (tool) {
    case PrintingTool::PrintSettings:
        return PrintingSettingsModel::tr("Add a printer, print a test page, and view or cancel jobs in Print Settings.");
    case PrintingTool::CupsAdministration:
        return PrintingSettingsModel::tr("Manage the CUPS print server in its web interface. The server must be available.");
    case PrintingTool::DocumentScanner:
        return PrintingSettingsModel::tr("Choose a scanner and save documents as PDF or images in Document Scanner.");
    }
    return {};
}
QString reason(const PrintingTool tool, const PrintingToolAvailability availability) {
    if (availability == PrintingToolAvailability::Available) return {};
    if (availability == PrintingToolAvailability::Unsupported)
        return PrintingSettingsModel::tr("The installed tool cannot be launched here. Check its desktop entry, then refresh.");
    switch (tool) {
    case PrintingTool::PrintSettings: return PrintingSettingsModel::tr("Print Settings was not found. Install or repair the application, then refresh.");
    case PrintingTool::CupsAdministration: return PrintingSettingsModel::tr("The CUPS administration entry was not found. Install or repair CUPS, then refresh.");
    case PrintingTool::DocumentScanner: return PrintingSettingsModel::tr("Document Scanner was not found. Install or repair the application, then refresh.");
    }
    return PrintingSettingsModel::tr("This tool is unavailable.");
}
QString plainName(QString text) {
    text.replace(QChar::Null, QChar::ReplacementCharacter);
    return text.left(128);
}
bool boundedProcessPlan(const PreparedPrintingTool &plan) {
    if (plan.program.isEmpty() || plan.program.size() > 4096
        || plan.program.contains(QChar::Null) || plan.arguments.size() > 128) return false;
    qsizetype total = plan.program.size();
    for (const auto &argument : plan.arguments) {
        if (argument.size() > 4096 || argument.contains(QChar::Null)
            || total > 65536 - argument.size()) return false;
        total += argument.size();
    }
    return true;
}
}
PrintingSettingsModel::PrintingSettingsModel(PrintingToolCatalog &catalog,
    PrintingArgvStarter &starter, QObject *parent)
    : QObject(parent), m_catalog(catalog), m_starter(starter) {
    refresh();
}
QVariantList PrintingSettingsModel::tools() const {
    QVariantList rows;
    for (std::size_t index = 0; index < PrintingToolCount; ++index) {
        const auto tool = static_cast<PrintingTool>(index);
        const auto &state = m_tools[index];
        rows.append(QVariantMap{
            {QStringLiteral("id"), printingToolId(tool)},
            {QStringLiteral("title"), title(tool)},
            {QStringLiteral("description"), description(tool)},
            {QStringLiteral("actionText"), actionText(tool)},
            {QStringLiteral("available"), state.availability == PrintingToolAvailability::Available},
            {QStringLiteral("applicationName"), plainName(state.applicationName)},
            {QStringLiteral("reason"), reason(tool, state.availability)},
        });
    }
    return rows;
}
void PrintingSettingsModel::setBusy(const bool busy) {
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}
void PrintingSettingsModel::setStatus(const QString &status) {
    if (m_statusText == status) return;
    m_statusText = status;
    emit statusTextChanged();
}
void PrintingSettingsModel::refresh() {
    Q_ASSERT(thread() == QThread::currentThread());
    if (m_busy) return;
    setBusy(true);
    m_tools = m_catalog.inspect();
    emit toolsChanged();
    setStatus({});
    setBusy(false);
}
void PrintingSettingsModel::launchTool(const QString &id) {
    Q_ASSERT(thread() == QThread::currentThread());
    const auto tool = printingToolForId(id);
    if (m_busy || !tool) return;
    setBusy(true);
    auto prepared = m_catalog.prepare(*tool);
    if (prepared.state.availability == PrintingToolAvailability::Available
        && !boundedProcessPlan(prepared))
        prepared.state.availability = PrintingToolAvailability::Unsupported;
    m_tools[static_cast<std::size_t>(*tool)] = prepared.state;
    emit toolsChanged();
    if (prepared.state.availability != PrintingToolAvailability::Available
        || !boundedProcessPlan(prepared)) {
        setStatus(tr("The tool is unavailable. Refresh after checking the installed application."));
    } else if (!m_starter.start(prepared.program, prepared.arguments)) {
        setStatus(tr("The tool could not be started. Check the installed application, then try again."));
    } else {
        // AGENT-GUARD: a successful start is submission only. Printer/service,
        // scan and job success remain in the existing owner UI (ADR-0356).
        setStatus(tr("Launch requested. Connection, device and job status are shown in the application."));
    }
    setBusy(false);
}
} // namespace QindaQt::Apps::SettingsPrinting
