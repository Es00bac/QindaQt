// SPDX-License-Identifier: GPL-3.0-or-later
// The obs-qindaqt module entry points (ADR-0208). Source types register at
// load; the bridge itself starts after every module has loaded, on OBS's Qt
// main thread, so obs-websocket's vendor API is available and the frontend
// can be told when to expect the console's sources.
#include "bridge_controller.h"
#include "console_source_types.h"
#include "websocket_vendor.h"

#include <obs-module.h>

#include <QCoreApplication>

#include <memory>

OBS_DECLARE_MODULE()

namespace {

std::unique_ptr<QindaQt::ObsBridge::BridgeController> g_controller;
std::unique_ptr<QindaQt::ObsBridge::WebSocketVendor> g_vendor;

} // namespace

MODULE_EXPORT const char *obs_module_name(void)
{
    return "obs-qindaqt";
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return "QindaQt console buses and strips as OBS audio sources";
}

bool obs_module_load(void)
{
    QindaQt::ObsBridge::registerConsoleSourceTypes();
    blog(LOG_INFO, "[obs-qindaqt] loaded: bridge %d on libobs %s",
         QindaQt::ObsBridge::BridgeVersion, obs_get_version_string());
    return true;
}

void obs_module_post_load(void)
{
    if (QCoreApplication::instance() == nullptr) {
        // No Qt event loop (a headless libobs host): the source types work,
        // the live bridge has nothing to run on.
        blog(LOG_INFO, "[obs-qindaqt] no Qt application; the console bridge stays off");
        return;
    }
    g_controller = std::make_unique<QindaQt::ObsBridge::BridgeController>();
    g_controller->start();
    g_vendor = std::make_unique<QindaQt::ObsBridge::WebSocketVendor>(*g_controller);
}

void obs_module_unload(void)
{
    g_vendor.reset();
    if (g_controller) {
        g_controller->stop();
        g_controller.reset();
    }
}
