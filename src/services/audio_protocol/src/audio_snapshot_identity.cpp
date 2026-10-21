// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_protocol/audio_snapshot_identity.h>

namespace QindaQt::Audio
{
namespace
{

// The snapshot as its revision identifies it: meter readings are a stream.
[[nodiscard]] Snapshot withoutMeterReadings(Snapshot snapshot)
{
    for (Strip &strip : snapshot.console.strips) {
        strip.level = Level{};
    }
    for (Bus &bus : snapshot.console.buses) {
        bus.level = Level{};
    }
    return snapshot;
}

} // namespace

bool sameRevisionContent(const Snapshot &left, const Snapshot &right)
{
    return withoutMeterReadings(left) == withoutMeterReadings(right);
}

} // namespace QindaQt::Audio
