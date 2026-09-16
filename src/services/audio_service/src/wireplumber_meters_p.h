// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_console.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

#include <atomic>
#include <memory>
#include <unordered_map>

struct pw_context;

namespace QindaQt::Audio
{

// AGENT-CONTRACT: level metering for the console (ADR-0173). PipeWire has no
// "give me this node's level" API and WirePlumber exposes none either, so a
// meter is a real capture stream: one `pw_stream` per metered node that reads
// the audio and computes peak and RMS from the samples.
//
// Threading is the whole hazard here. `on_process` runs on PipeWire's REALTIME
// data thread, while readings are consumed on the service's worker loop. The
// only state crossing that boundary is a pair of lock-free atomics per stream,
// and the RT side never allocates, never locks and never touches Qt.
class MeterBank final
{
public:
    // Realtime-facing per-node state. Public only because PipeWire's callback
    // takes a void* and the callback is a free function.
    struct Stream;

    struct Target {
        // The console id this meter belongs to (`strip.hw.1`, `bus.a1`).
        QString consoleId;
        // The PipeWire node to read.
        QString nodeName;
        // True when the node is a sink, whose audio must be taken from its
        // monitor rather than treated as a source.
        bool captureSink = false;
        // A passive meter never causes a suspended device to resume: it reads
        // only while something else is already using that device.
        //
        // AGENT-GUARD: this is a consent boundary, not a performance knob. A
        // non-passive capture stream OPENS the device - it switches on the
        // user's microphone and lights the recording indicator on their camera.
        // Nothing may make a capture meter non-passive without the user having
        // asked for that input to be live.
        bool passive = true;
    };

    MeterBank();
    ~MeterBank();
    MeterBank(const MeterBank &) = delete;
    MeterBank &operator=(const MeterBank &) = delete;

    // Declares the complete set of nodes to meter. Streams for targets that are
    // gone are destroyed and new ones created; a target whose stream already
    // exists is left running, so a console change does not interrupt metering
    // that is already working.
    void configure(pw_context *context, const QList<Target> &targets);
    // Latest reading per console id, in dBFS. Reading CLEARS the peak hold, so
    // each call reports the loudest sample since the previous call rather than
    // an instantaneous value that would miss transients between polls.
    [[nodiscard]] QHash<QString, Level> takeReadings();
    void clear();

private:
    // Not QHash: the value is move-only, and QHash requires copyable values
    // for its implicit sharing.
    std::unordered_map<QString, std::unique_ptr<Stream>> m_streams;
};

} // namespace QindaQt::Audio
