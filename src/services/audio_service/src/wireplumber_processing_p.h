// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_protocol/audio_console.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>

namespace QindaQt::Audio
{

// A strip's rack as one libpipewire-module-filter-chain (ADR-0179): captures
// the strip's device, runs the enabled blocks in order, and presents the
// result as a virtual SOURCE the sends and the meter read instead.
//
// AGENT-CONTRACT: the plugin labels and control names below are the swh
// LADSPA plugins' own, verified with `analyseplugin`; the equalizer is
// PipeWire's builtin biquads. A rename on either side is a silent chain that
// PipeWire refuses to load, so tests pin every one of them.

// The virtual source the chain presents for a strip.
[[nodiscard]] QString processedNodeName(const QString &stripId);
// The chain node's own name, for by-name lookup when applying controls.
[[nodiscard]] QString processingChainNodeName(const QString &stripId);
// True when at least one block is enabled: a rack with every block off has no
// chain at all, and the sends read the device directly.
[[nodiscard]] bool processingActive(const StripProcessing &processing);
// Module arguments, or empty when a name cannot be embedded safely.
[[nodiscard]] QByteArray processingModuleArguments(const QString &stripId,
                                                   const QString &sourceNodeName,
                                                   bool sourceIsSink,
                                                   const StripProcessing &processing);
// The bus rack (ADR-0180): captures the bus's own sink and plays into the
// device the bus drives, with one equalizer per channel and the channel mode
// as the output mapping. Only a physical bus has one; a virtual bus's sink IS
// what other applications record.
[[nodiscard]] QString busChainNodeName(const QString &busId);
// busProcessingActive() is the protocol's (audio_validation.h).
[[nodiscard]] QByteArray busProcessingModuleArguments(const QString &busId,
                                                      const QString &deviceNodeName,
                                                      const BusProcessing &processing);
// The chain's control values as "<node>:<control>" -> value, for updating a
// running chain without rebuilding it. Only enabled blocks are listed.
[[nodiscard]] QList<QPair<QByteArray, double>>
processingControls(const StripProcessing &processing);

} // namespace QindaQt::Audio
