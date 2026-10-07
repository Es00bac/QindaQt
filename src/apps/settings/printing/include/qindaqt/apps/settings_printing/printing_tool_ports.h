// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <array>
#include <cstddef>
#include <optional>

namespace QindaQt::Apps::SettingsPrinting {

enum class PrintingTool { PrintSettings, CupsAdministration, DocumentScanner };
inline constexpr std::size_t PrintingToolCount = 3;
enum class PrintingToolAvailability { Available, Missing, Unsupported };

struct PrintingToolState final {
    PrintingToolAvailability availability = PrintingToolAvailability::Missing;
    QString applicationName;
};
struct PreparedPrintingTool final {
    PrintingToolState state;
    QString program;
    QStringList arguments;
};
using PrintingToolInventory = std::array<PrintingToolState, PrintingToolCount>;

[[nodiscard]] QString printingToolId(PrintingTool tool);
[[nodiscard]] std::optional<PrintingTool> printingToolForId(const QString &id);

// Borrowed synchronous GUI-thread port. inspect reads bounded installed metadata
// only. prepare must freshly resolve the same fixed identity before a deliberate
// launch; it cannot reuse an inventory-time argv after removal/replacement.
// No printer, scanner, service, package, or job operation belongs to this port.
class PrintingToolCatalog {
public:
    virtual ~PrintingToolCatalog() = default;
    [[nodiscard]] virtual PrintingToolInventory inspect() const = 0;
    [[nodiscard]] virtual PreparedPrintingTool prepare(PrintingTool tool) const = 0;
};

// Borrowed GUI-thread literal argv port; only a fixed catalog-validated process
// plan reaches it. true acknowledges startup submission, never a mapped window,
// CUPS/device readiness, or completed job. No shell or terminal fallback.
class PrintingArgvStarter {
public:
    virtual ~PrintingArgvStarter() = default;
    [[nodiscard]] virtual bool start(const QString &program,
                                     const QStringList &arguments) = 0;
};

} // namespace QindaQt::Apps::SettingsPrinting
