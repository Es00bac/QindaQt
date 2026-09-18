// SPDX-License-Identifier: GPL-3.0-or-later
#include "websocket_vendor.h"

#include "bridge_controller.h"

#include <obs.h>
#if defined(QINDAQT_OBS_WEBSOCKET_API)
#include <obs-websocket-api.h>
#endif

#include <QObject>

namespace QindaQt::ObsBridge {
namespace {

obs_data_t *entryFor(const DesiredSource &source)
{
    obs_data_t *entry = obs_data_create();
    obs_data_set_string(entry, "consoleId", source.consoleId.toUtf8().constData());
    obs_data_set_string(entry, "code", source.code.toUtf8().constData());
    obs_data_set_string(entry, "label", source.label.toUtf8().constData());
    obs_data_set_string(entry, "sourceName", source.sourceName.toUtf8().constData());
    obs_data_set_string(entry, "sourceKind", sourceKindId(source.kind).toUtf8().constData());
    obs_data_set_string(entry, "captureKind",
                        captureKindToken(source.captureKind).toUtf8().constData());
    obs_data_set_string(entry, "captureDevice", source.captureDevice.toUtf8().constData());
    obs_data_set_bool(entry, "muted", source.muted);
    obs_data_set_double(entry, "gainDb", source.gainDb);
    return entry;
}

void fillMapping(obs_data_t *data, const ConsoleMapping &mapping)
{
    obs_data_array_t *buses = obs_data_array_create();
    obs_data_array_t *strips = obs_data_array_create();
    for (const DesiredSource &source : mapping.sources) {
        obs_data_t *entry = entryFor(source);
        obs_data_array_push_back(source.kind == SourceKind::Bus ? buses : strips, entry);
        obs_data_release(entry);
    }
    obs_data_set_array(data, "buses", buses);
    obs_data_set_array(data, "strips", strips);
    obs_data_array_release(buses);
    obs_data_array_release(strips);
    obs_data_set_int(data, "bridgeVersion", BridgeVersion);
    obs_data_set_string(data, "audioState", mapping.audioState.toUtf8().constData());
    obs_data_set_string(data, "reasonCode", mapping.reasonCode.toUtf8().constData());
    obs_data_set_int(data, "epoch", static_cast<long long>(mapping.epoch));
    obs_data_set_int(data, "revision", static_cast<long long>(mapping.revision));
}

} // namespace

WebSocketVendor::WebSocketVendor(BridgeController &controller)
    : m_controller(controller)
{
#if defined(QINDAQT_OBS_WEBSOCKET_API)
    if (obs_websocket_get_api_version() == 0) {
        blog(LOG_INFO, "[obs-qindaqt] obs-websocket is not loaded; no vendor API");
        return;
    }
    m_vendor = obs_websocket_register_vendor(VendorName);
    if (m_vendor == nullptr) {
        blog(LOG_WARNING, "[obs-qindaqt] obs-websocket refused the '%s' vendor", VendorName);
        return;
    }
    obs_websocket_vendor_register_request(
        m_vendor, MappingRequest,
        [](obs_data_t *request, obs_data_t *response, void *priv) {
            handleMapping(request, response, priv);
        },
        this);
    m_connection = QObject::connect(&m_controller, &BridgeController::mappingChanged,
                                    [this] { emitChanged(); });
    blog(LOG_INFO, "[obs-qindaqt] vendor '%s' registered (%s)", VendorName, MappingRequest);
#endif
}

WebSocketVendor::~WebSocketVendor()
{
#if defined(QINDAQT_OBS_WEBSOCKET_API)
    if (m_connection) {
        QObject::disconnect(m_connection);
    }
    if (m_vendor != nullptr) {
        obs_websocket_vendor_unregister_request(m_vendor, MappingRequest);
    }
#endif
}

void WebSocketVendor::handleMapping(void *, void *response, void *priv)
{
    auto *self = static_cast<WebSocketVendor *>(priv);
    // Called on obs-websocket's thread: the mapping is a locked copy.
    fillMapping(static_cast<obs_data_t *>(response), self->m_controller.mapping());
}

void WebSocketVendor::emitChanged()
{
#if defined(QINDAQT_OBS_WEBSOCKET_API)
    if (m_vendor == nullptr) {
        return;
    }
    obs_data_t *data = obs_data_create();
    fillMapping(data, m_controller.mapping());
    obs_websocket_vendor_emit_event(m_vendor, MappingChangedEvent, data);
    obs_data_release(data);
#endif
}

} // namespace QindaQt::ObsBridge
