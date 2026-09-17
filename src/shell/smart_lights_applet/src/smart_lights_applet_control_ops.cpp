// SPDX-License-Identifier: GPL-3.0-or-later

#include "smart_lights_applet_controller.h"

#include <QtCore/QCoreApplication>

namespace QindaQt::Shell::SmartLightsApplet
{
namespace
{

[[nodiscard]] QString translate(const char *text)
{
    return QCoreApplication::translate("QindaQt::Shell::SmartLightsApplet", text);
}

[[nodiscard]] quint8 clampChannel(const int value)
{
    return static_cast<quint8>(qBound(0, value, 255));
}

} // namespace

bool SmartLightsAppletController::requestPower(const QString &rowId, const bool on)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty()) {
        return false;
    }
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::SetPower;
    request.targetMac = mac;
    request.state.setPower = true;
    request.state.on = on;
    return dispatch(request);
}

bool SmartLightsAppletController::requestAllPower(const bool on)
{
    bool dispatched = false;
    // Each light is its own operation: one unreachable luminaire must not stop
    // the rest of the room from answering.
    for (const DeviceRow &row : m_model.devices) {
        if (!row.controllable || row.on == on) {
            continue;
        }
        if (requestPower(row.id, on)) {
            dispatched = true;
        }
    }
    if (!dispatched) {
        publishFeedback(on ? translate("No light is available to switch on.")
                           : translate("No light is available to switch off."));
    }
    return dispatched;
}

bool SmartLightsAppletController::requestBrightness(const QString &rowId,
                                                    const int percent)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty()) {
        return false;
    }
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::SetBrightness;
    request.targetMac = mac;
    request.state.setDimming = true;
    request.state.dimmingPercent = static_cast<quint8>(qBound(1, percent, 100));
    return dispatch(request);
}

bool SmartLightsAppletController::requestTemperature(const QString &rowId,
                                                     const int kelvin)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty()) {
        return false;
    }
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::SetTemperature;
    request.targetMac = mac;
    request.state.setTemperature = true;
    request.state.temperatureKelvin =
        static_cast<quint16>(qBound(1000, kelvin, 10000));
    return dispatch(request);
}

bool SmartLightsAppletController::requestColor(const QString &rowId, const int red,
                                               const int green, const int blue)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty()) {
        return false;
    }
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::SetColor;
    request.targetMac = mac;
    request.state.setColor = true;
    request.state.red = clampChannel(red);
    request.state.green = clampChannel(green);
    request.state.blue = clampChannel(blue);
    // The white channels stay dark: mixing them into a chosen colour is what
    // makes a saturated hue look washed out on these luminaires.
    request.state.coolWhite = 0;
    request.state.warmWhite = 0;
    return dispatch(request);
}

bool SmartLightsAppletController::requestScene(const QString &rowId, const int sceneId)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty()) {
        return false;
    }
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::SetScene;
    request.targetMac = mac;
    request.state.setScene = true;
    request.state.sceneId = static_cast<quint16>(qBound(0, sceneId, 1000));
    return dispatch(request);
}

bool SmartLightsAppletController::requestSpeed(const QString &rowId, const int percent)
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty()) {
        return false;
    }
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::SetSpeed;
    request.targetMac = mac;
    request.state.setSpeed = true;
    request.state.speedPercent = static_cast<quint8>(qBound(10, percent, 200));
    return dispatch(request);
}

bool SmartLightsAppletController::requestRefresh()
{
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::Refresh;
    return dispatch(request);
}

bool SmartLightsAppletController::requestDiscovery()
{
    Wiz::OperationRequest request;
    request.kind = Wiz::OperationKind::Discover;
    return dispatch(request);
}

} // namespace QindaQt::Shell::SmartLightsApplet
