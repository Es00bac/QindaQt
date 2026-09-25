// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinoutputmodes.h"

#include <core/backendoutput.h>
#include <core/output.h>

#include <algorithm>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool advertisable(const KWin::OutputMode &mode)
{
    return !mode.isRemoved() && mode.size().width() > 0 && mode.size().height() > 0
        && mode.size().width() <= OutputInventoryStore::MaxModePixelDimension
        && mode.size().height() <= OutputInventoryStore::MaxModePixelDimension
        && mode.refreshRate() > 0
        && mode.refreshRate() <= OutputInventoryStore::MaxRefreshRateMilliHz;
}

} // namespace

QVector<OutputInventoryMode> sampleAdvertisedModes(const KWin::BackendOutput &output)
{
    const auto sameMode = [](const OutputInventoryMode &entry, const KWin::OutputMode &mode) {
        return entry.pixelSize == mode.size() && entry.refreshRateMilliHz == mode.refreshRate();
    };
    const auto entryFor = [](const KWin::OutputMode &mode) {
        return OutputInventoryMode{
            .pixelSize = mode.size(),
            .refreshRateMilliHz = mode.refreshRate(),
            .preferred = mode.flags().testFlag(KWin::OutputMode::Flag::Preferred)};
    };
    QVector<OutputInventoryMode> modes;
    for (const auto &mode : output.modes()) {
        if (!mode || !advertisable(*mode)) {
            continue;
        }
        const auto existing = std::ranges::find_if(
            modes, [&](const OutputInventoryMode &entry) { return sameMode(entry, *mode); });
        if (existing != modes.end()) {
            // DRM advertises distinct timings (e.g. reduced blanking) that
            // Display1 cannot tell apart; keep one entry per size+refresh.
            existing->preferred = existing->preferred || entryFor(*mode).preferred;
        } else if (modes.size() < OutputInventoryStore::MaxModes) {
            modes.append(entryFor(*mode));
        }
    }
    // AGENT-GUARD: Display1 requires the selected mode to appear in the
    // advertised list. If truncation dropped it, it takes the last slot.
    const auto current = output.currentMode();
    if (current && advertisable(*current)
        && std::ranges::none_of(modes, [&](const OutputInventoryMode &entry) {
               return sameMode(entry, *current);
           })) {
        if (modes.size() >= OutputInventoryStore::MaxModes) {
            modes.removeLast();
        }
        modes.append(entryFor(*current));
    }
    return modes;
}

QString sampleReplicationSource(const KWin::BackendOutput &output,
                              const QVector<const KWin::BackendOutput *> &published)
{
    // KWin stores the mirror source as the source's runtime UUID. Display1
    // identifies outputs by connector, so resolve it within the same sample;
    // an unresolvable source (disconnected, non-desktop) is reported as none.
    const QString sourceUuid = output.replicationSource();
    if (sourceUuid.isEmpty() || !output.isEnabled()) {
        return {};
    }
    for (const auto *candidate : published) {
        if (candidate && candidate != &output && candidate->uuid() == sourceUuid
            && candidate->isEnabled() && candidate->replicationSource().isEmpty()) {
            return candidate->name();
        }
    }
    return {};
}

} // namespace QindaQt::Compositor::KWinIntegration
