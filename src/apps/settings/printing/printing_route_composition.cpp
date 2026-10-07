// SPDX-License-Identifier: GPL-3.0-or-later
#include "printing_route_composition.h"
#include <qindaqt/apps/settings_printing/application_printing_tool_catalog.h>
#include <qindaqt/apps/settings_printing/printing_settings_model.h>
#include <QProcess>
#include <QStandardPaths>

namespace QindaQt::Apps::SettingsPrinting {
namespace {
class ProcessPrintingArgvStarter final : public PrintingArgvStarter {
public:
    bool start(const QString &program, const QStringList &arguments) override {
        return QProcess::startDetached(program, arguments);
    }
};
}
class PrintingRouteComposition::Private final {
public:
    Private()
        : catalog(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)),
          model(catalog, starter) {}
    // Constructor order is the borrowed-port lifetime contract, independent of
    // the active route Loader. Responsive host reconstruction cannot add effects.
    ApplicationPrintingToolCatalog catalog;
    ProcessPrintingArgvStarter starter;
    PrintingSettingsModel model;
};
PrintingRouteComposition::PrintingRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}
PrintingRouteComposition::~PrintingRouteComposition() = default;
QObject *PrintingRouteComposition::model() const { return &d->model; }
}
