// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QString>

namespace QindaQt::Audio
{

// AGENT-CONTRACT: builds the PipeWire arguments for one console send
// (ADR-0173). Pure string construction, separated from the worker so the
// argument shape is testable without a running daemon - a malformed module
// argument otherwise fails silently inside PipeWire with nothing to inspect.
//
// A send is realised as one `libpipewire-module-loopback`: it captures from the
// strip's node and plays into the bus's node, and it owns its own volume, which
// is what gives every matrix cell an independent gain. A plain port link cannot
// carry a gain, which is why links are not used here.

// Deterministic node name for one send, so a restart can recognise the
// loopbacks it previously created instead of duplicating them.
[[nodiscard]] QString routingNodeName(const QString &stripId, const QString &busId);

// The module argument string. `sourceNodeName` and `targetNodeName` are the
// PipeWire node names of the strip's and the bus's devices; `linear` is the
// send's gain already converted out of decibels.
//
// AGENT-GUARD: node names reach PipeWire inside a JSON-ish argument. They are
// quoted and rejected if they contain a quote or backslash, because a crafted
// device name would otherwise be able to inject additional properties into the
// module argument.
[[nodiscard]] QByteArray routingModuleArguments(const QString &stripId,
                                                const QString &busId,
                                                const QString &sourceNodeName,
                                                const QString &targetNodeName,
                                                // True only when the strip's
                                                // source is itself a sink, so
                                                // its audio is on the monitor.
                                                bool sourceIsSink, double linear);

// True when a node name is safe to embed in a module argument.
[[nodiscard]] bool routingNameIsEmbeddable(const QString &nodeName);

} // namespace QindaQt::Audio
