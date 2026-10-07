// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/apps/settings_printing/printing_tool_ports.h>
#include <functional>
#include <QList>
#include <utility>

namespace QindaQt::Apps::SettingsPrinting::Test {
class FakePrintingCatalog final : public PrintingToolCatalog {
public:
    PrintingToolInventory inventory;
    std::array<PreparedPrintingTool, PrintingToolCount> preparations;
    mutable int inspections = 0;
    mutable int preparationsRead = 0;
    void available(PrintingTool tool, bool value = true) {
        const auto index = static_cast<std::size_t>(tool);
        inventory[index] = {value ? PrintingToolAvailability::Available
                                  : PrintingToolAvailability::Missing,
                             QStringLiteral("Fixture owner")};
        preparations[index] = {inventory[index], QStringLiteral("/fixture/owner"),
                              {printingToolId(tool)}};
    }
    PrintingToolInventory inspect() const override {
        ++inspections;
        return inventory;
    }
    PreparedPrintingTool prepare(PrintingTool tool) const override {
        ++preparationsRead;
        return preparations[static_cast<std::size_t>(tool)];
    }
};
class FakePrintingStarter final : public PrintingArgvStarter {
public:
    QList<std::pair<QString, QStringList>> calls;
    bool submitted = true;
    std::function<void()> duringStart;
    bool start(const QString &program, const QStringList &arguments) override {
        calls.append(std::make_pair(program, arguments));
        if (duringStart) duringStart();
        return submitted;
    }
};
}
