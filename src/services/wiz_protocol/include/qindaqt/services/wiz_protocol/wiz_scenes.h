// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <optional>

namespace QindaQt::Wiz
{

// One vendor-assigned lighting programme.
//
// AGENT-CONTRACT: `id` is the firmware's sceneId and is part of the stored
// configuration format. Never renumber an entry; a stored preset would then
// select a different programme after an upgrade. Names may be retranslated.
struct SceneDescriptor {
    quint16 id = 0;
    QString name;
    // The programme animates over time, so playback speed applies to it.
    bool dynamic = false;
    // Channels the luminaire must have for the programme to render as designed.
    Features required;

    friend bool operator==(const SceneDescriptor &, const SceneDescriptor &) = default;
};

// The complete vendor scene table in firmware order.
[[nodiscard]] QList<SceneDescriptor> allScenes();

// The subset a luminaire with `features` can actually run, in firmware order.
[[nodiscard]] QList<SceneDescriptor> scenesFor(Features features);

[[nodiscard]] std::optional<SceneDescriptor> sceneById(quint16 id);

// True when `features` covers everything the scene needs. An unknown id is
// never supported.
[[nodiscard]] bool sceneSupported(quint16 id, Features features);

} // namespace QindaQt::Wiz
