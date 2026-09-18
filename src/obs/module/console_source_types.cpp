// SPDX-License-Identifier: GPL-3.0-or-later
#include "console_source_types.h"

#include "qindaqt/obs_bridge/console_sources.h"

#include <obs-module.h>
#include <obs.h>

#include <QString>

#include <cstddef>
#include <string>

namespace QindaQt::ObsBridge {
namespace {

// One console source: the OBS source it is (`self`) and the private capture
// source that actually reads the PipeWire node (`child`).
struct ConsoleSource {
    obs_source_t *self = nullptr;
    obs_source_t *child = nullptr;
    std::string captureDevice;
    CaptureKind captureKind = CaptureKind::None;
};

// The child's audio, already in the audio subsystem's mixing format, is
// republished as this source's own output.
void forwardAudio(void *param, obs_source_t *, const struct audio_data *audio, bool muted)
{
    auto *source = static_cast<ConsoleSource *>(param);
    if (muted || audio == nullptr || audio->frames == 0 || source->self == nullptr) {
        return;
    }
    const struct audio_output_info *info = audio_output_get_info(obs_get_audio());
    if (info == nullptr) {
        return;
    }
    struct obs_source_audio out = {};
    for (std::size_t plane = 0; plane < MAX_AV_PLANES; ++plane) {
        out.data[plane] = audio->data[plane];
    }
    out.frames = audio->frames;
    out.speakers = info->speakers;
    out.format = info->format;
    out.samples_per_sec = info->samples_per_sec;
    out.timestamp = audio->timestamp;
    obs_source_output_audio(source->self, &out);
}

void releaseChild(ConsoleSource *source)
{
    if (source->child == nullptr) {
        return;
    }
    obs_source_remove_audio_capture_callback(source->child, forwardAudio, source);
    obs_source_remove_active_child(source->self, source->child);
    obs_source_release(source->child);
    source->child = nullptr;
}

void createChild(ConsoleSource *source)
{
    releaseChild(source);
    if (source->captureKind == CaptureKind::None || source->captureDevice.empty()) {
        return;
    }
    // PipeWire serves the PulseAudio protocol, so the console's nodes are
    // reachable through OBS's own PulseAudio capture sources: a real source
    // node through input capture, a sink's monitor through output capture.
    const char *childId = source->captureKind == CaptureKind::Input ? "pulse_input_capture"
                                                                     : "pulse_output_capture";
    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "device_id", source->captureDevice.c_str());
    const std::string childName = std::string(obs_source_get_name(source->self)) + " (capture)";
    source->child = obs_source_create_private(childId, childName.c_str(), settings);
    obs_data_release(settings);
    if (source->child == nullptr) {
        blog(LOG_WARNING, "[obs-qindaqt] capture type '%s' is unavailable; '%s' stays silent",
             childId, obs_source_get_name(source->self));
        return;
    }
    obs_source_add_audio_capture_callback(source->child, forwardAudio, source);
    // The child follows this source's activation, so it captures exactly
    // while OBS mixes this source.
    obs_source_add_active_child(source->self, source->child);
}

void readSettings(ConsoleSource *source, obs_data_t *settings)
{
    source->captureDevice = obs_data_get_string(settings, SettingsKeys::CaptureDevice);
    source->captureKind = captureKindFromToken(QString::fromUtf8(
                                                   obs_data_get_string(settings, SettingsKeys::CaptureKind)))
                              .value_or(CaptureKind::None);
}

void *createSource(obs_data_t *settings, obs_source_t *self)
{
    auto *source = new ConsoleSource;
    source->self = self;
    readSettings(source, settings);
    createChild(source);
    return source;
}

void destroySource(void *data)
{
    auto *source = static_cast<ConsoleSource *>(data);
    releaseChild(source);
    delete source;
}

void updateSource(void *data, obs_data_t *settings)
{
    auto *source = static_cast<ConsoleSource *>(data);
    const std::string deviceBefore = source->captureDevice;
    const CaptureKind kindBefore = source->captureKind;
    readSettings(source, settings);
    if (deviceBefore != source->captureDevice || kindBefore != source->captureKind) {
        createChild(source);
    }
}

const char *busTypeName(void *)
{
    return "QindaQt Console Bus";
}

const char *stripTypeName(void *)
{
    return "QindaQt Console Strip";
}

// The properties dialog shows the bridge's facts read-only: the console owns
// them, and Settings is where a bus or strip is renamed or repointed.
obs_properties_t *sourceProperties(void *)
{
    obs_properties_t *properties = obs_properties_create();
    const struct {
        const char *key;
        const char *label;
    } rows[] = {
        {SettingsKeys::ConsoleId, "Console id"},
        {SettingsKeys::Code, "Console code"},
        {SettingsKeys::Label, "Console label"},
        {SettingsKeys::CaptureKind, "Capture"},
        {SettingsKeys::CaptureDevice, "PipeWire node"},
    };
    for (const auto &row : rows) {
        obs_property_t *property =
            obs_properties_add_text(properties, row.key, row.label, OBS_TEXT_DEFAULT);
        obs_property_set_enabled(property, false);
    }
    return properties;
}

struct obs_source_info sourceInfo(const char *id, const char *(*typeName)(void *))
{
    struct obs_source_info info = {};
    info.id = id;
    info.type = OBS_SOURCE_TYPE_INPUT;
    // Audio only; never duplicated into a second scene item; hidden from the
    // "add source" menu because the bridge creates and removes these itself.
    info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_CAP_DISABLED;
    info.get_name = typeName;
    info.create = createSource;
    info.destroy = destroySource;
    info.update = updateSource;
    info.get_properties = sourceProperties;
    info.icon_type = OBS_ICON_TYPE_AUDIO_INPUT;
    return info;
}

} // namespace

void registerConsoleSourceTypes()
{
    static struct obs_source_info bus = sourceInfo(BusSourceId, busTypeName);
    static struct obs_source_info strip = sourceInfo(StripSourceId, stripTypeName);
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    obs_register_source(&bus);
    obs_register_source(&strip);
}

} // namespace QindaQt::ObsBridge
