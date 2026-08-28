// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/power_applet/power_applet_types.h>

namespace QindaQt::Shell::PowerApplet::detail {

// AGENT-GUARD: Power1 and Brightness enumerations have a fixed quint32
// underlying type, so raw comparison is well defined for any input, including
// hostile hand-built values. Every mapping helper must range-check the raw
// value before a typed switch; switching on an out-of-vocabulary enumerator
// is undefined behavior, and a presentation layer must never crash on
// hostile input.
template <typename Enum>
bool inVocabulary(const Enum value, const quint32 maximum) {
  const quint32 raw = static_cast<quint32>(value);
  return raw <= maximum;
}

// Appends `phrase` to `base` as ", phrase" unless the phrase is empty. Keeps
// accessible sentences complete without dangling separators.
[[nodiscard]] QString appendPhrase(const QString &base, const QString &phrase);

// Projects the brightness control rows into `model` from the composed view
// when its owner is present, or as identity-visible, fail-closed fallback
// rows from the snapshot devices otherwise.
void projectControls(const Power::Snapshot &snapshot,
                     const BrightnessView &brightness, PowerAppletModel &model);

} // namespace QindaQt::Shell::PowerApplet::detail
