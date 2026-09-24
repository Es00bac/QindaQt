// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::Shell {

// AGENT-CONTRACT: The built-in layout a new user gets and every startup or
// live-adoption fallback lands on (ADR-0263). It must equal the
// panels.layoutProfile default in data/settings/schema-v2.json and in
// data/settings/profile-defaults/qindaqt.json, and name an installed stock
// profile under data/profiles/; tst_shellpreferencevalues checks all three.
inline constexpr auto DefaultLayoutProfileId = "macos-inspired";

} // namespace QindaQt::Shell
