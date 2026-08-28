// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <qindaqt/services/display_color_model/color_limits.h>
#include <qindaqt/services/display_color_model/color_model.h>
#include <qindaqt/services/display_color_model/color_types.h>
#include <qindaqt/services/display_color_model/color_validation.h>

using namespace QindaQt::DisplayColor;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    ColorModel model("installed-consumer-epoch");
    if (model.serviceEpoch() != "installed-consumer-epoch") {
        qCritical() << "Failed to initialize ColorModel epoch";
        return 1;
    }

    OutputColorCapabilities caps;
    caps.stableId = "DP-1";
    caps.supportsHdr = true;
    caps.supportsWcg = true;
    caps.supportedTransferFunctions = {TransferFunction::Srgb, TransferFunction::Pq};
    caps.minLuminanceNits = 0.05;
    caps.maxLuminanceNits = 1000.0;
    caps.maxFullFrameLuminanceNits = 600.0;
    caps.wireValid = true;

    if (!model.updateCapabilities(caps)) {
        qCritical() << "Failed to update capabilities in installed consumer";
        return 1;
    }

    const auto snap = model.snapshot();
    if (snap.outputs.isEmpty()) {
        qCritical() << "Expected output state in snapshot";
        return 1;
    }

    qInfo() << "Installed DisplayColorModel C++ consumer verified successfully";
    return 0;
}
