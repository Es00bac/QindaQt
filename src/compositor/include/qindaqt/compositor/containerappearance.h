// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>
#include <QtTypes>

namespace QindaQt::Compositor {

inline constexpr qsizetype ContainerNameMaximumCharacters = 255;

// AGENT-CONTRACT: Process-local, non-persisted presentation state for one
// window container, keyed by containerId elsewhere (HybridContainerAppearanceStore
// in src/compositor/kwin). This value type is the public boundary a future
// persistence owner (workspaces) reads/writes through; it never itself reaches
// into KWin, Core::WindowContainer, or TopologyCommand. An empty field means
// "no override": callers fall back to the derived title or theme accent.
struct ContainerAppearance final {
  QString name;
  QString colorHex;

  friend bool operator==(const ContainerAppearance &,
                         const ContainerAppearance &) = default;
};

// Trims surrounding whitespace, rejects control/format characters and
// malformed surrogates (hostile-controlled presentation text, same bound as
// other T0 identity/title fields), and caps length at
// ContainerNameMaximumCharacters. Returns an empty string for input that is
// empty after trimming or exceeds the bound; callers treat that as "no name
// override" rather than an error, since a rejected rename is a no-op, not a
// crash.
[[nodiscard]] QString normalizedContainerName(const QString &rawName);

// Accepts only exact "#RRGGBB" (uppercase hex digits after normalization).
[[nodiscard]] bool isValidContainerColor(const QString &colorHex);

// Uppercases and validates; returns an empty string when rawColor is empty or
// fails isValidContainerColor.
[[nodiscard]] QString normalizedContainerColor(const QString &rawColor);

struct ContainerColorSwatch final {
  // Doubles as the normalized "#RRGGBB" value and the stable menu/wire ID.
  QString colorHex;
  QString label;
};

// A fixed, curated palette. Not user-extensible; keeps the group-menu color
// submenu and any future dock legend deterministic across sessions.
[[nodiscard]] QVector<ContainerColorSwatch> containerColorSwatches();

} // namespace QindaQt::Compositor
