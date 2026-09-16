// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>

namespace QindaQt::Audio::ConsoleEndpoints
{

// AGENT-CONTRACT: the graph nodes a console's VIRTUAL strips and buses are made
// of (ADR-0175). One definition, used three ways: the service binds to these
// names, the worker creates them when the daemon does not already have them,
// and the shipped PipeWire drop-in declares the same nodes daemon-side. A test
// holds the drop-in to these definitions so the two cannot drift.
//
// A virtual strip is a null SINK: applications choose it as their output and
// the console reads its monitor, which is how one application gets its own
// fader.
//
// AGENT-GUARD: a strip's sink is created server-side through the raw
// pw_core_create_object API, never with wp_node_new_from_factory. The latter
// hands WirePlumber a WpNode proxy for the new global, and an object manager
// on the same core never presents a global whose proxy the core already owns:
// the sink exists in the daemon, every other client sees it, and this service
// alone cannot find it to bind. A raw proxy is not a WpProxy, so the global is
// presented like any foreign one; and with object.linger the sink outlives the
// proxy and the service, so an application playing into its strip keeps that
// output across an audio-service restart instead of landing on the speakers.
//
// A virtual bus is a loopback whose capture side is a SINK and whose playback
// side is a real SOURCE. The strips' sends play into the sink; applications
// record from the source. It has to be two typed nodes: WirePlumber will not
// link a playback stream into an Audio/Source/Virtual node even when asked by
// name, and browsers hide monitor sources from their microphone pickers, so a
// bus that was only a sink's monitor would be invisible to exactly the
// applications a streamer needs it in.

// Every node this module names starts with this. Deliberately distinct from the
// user-managed virtual-device prefix: console endpoints are not removable
// through RemoveVirtualDevice, and code that claims hardware must skip them.
inline constexpr char kConsoleNodeNamePrefix[] = "qindaqt.console.";

[[nodiscard]] bool isConsoleOwnedNodeName(const QString &nodeName);

// The sink applications play into for a virtual strip.
[[nodiscard]] QString stripSinkNodeName(const QString &stripId);
// The sink a virtual bus's sends play into.
[[nodiscard]] QString busSinkNodeName(const QString &busId);
// The source applications record a virtual bus from.
[[nodiscard]] QString busSourceNodeName(const QString &busId);

// Properties for a virtual strip's null sink, in order, for the "adapter"
// factory. Empty when the id or description is unusable.
[[nodiscard]] QList<QPair<QByteArray, QByteArray>>
stripSinkProperties(const QString &stripId, const QString &description);
// libpipewire-module-loopback arguments for a virtual bus. Empty when the id or
// description cannot be embedded safely.
[[nodiscard]] QByteArray busModuleArguments(const QString &busId,
                                            const QString &description);

} // namespace QindaQt::Audio::ConsoleEndpoints
