// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT: A TerminalProfile is the complete, validated description of
// how one terminal session is created and presented. It is pure data: no
// I/O, no Qt GUI types beyond value classes. The shell fields are a *command
// policy*, not an executable string: shellProgram/shellArguments are handed
// verbatim to TerminalLaunchPolicy::resolveShell at session creation, so a
// profile can never widen what the launch policy admits. A profile never
// carries session content or scrollback; see docs/wiki/apps/terminal.md.
struct TerminalProfile final {
  // Stable identity persisted in Settings1. Built-in profiles use a fixed
  // id; user profiles get a fresh UUID-derived id at creation time.
  QString id;
  // Visible name, 1..kMaxNameLength printable characters.
  QString name;
  // Empty inherits the launch default (CLI --shell or the built-in
  // default). Otherwise an absolute program path validated by the launch
  // policy at creation time (existence/executability checks happen there).
  QString shellProgram;
  // Verbatim arguments, bounded exactly like the CLI --arg contract.
  QStringList shellArguments;
  // Empty selects the theme projection's monospace font; otherwise an
  // installed font family name (QFont performs runtime matching).
  QString fontFamily;
  // 0 selects the theme projection's size; otherwise kMinFontSize..
  // kMaxFontSize.
  int fontSize = 0;
  // QindaQt theme identifier whose QST projection renders the terminal
  // surface (the Konsole scheme document). Default "qinda-dark".
  QString colorSchemeId;
  // Bounded scrollback in lines, 0..kMaxScrollbackLines.
  int scrollbackLines = kDefaultScrollbackLines;

  enum class BellPolicy { Silent, Audible };
  // Silent strips BEL bytes from forwarded child output before they reach
  // the rendering widget; Audible passes them through to the widget bell.
  BellPolicy bellPolicy = BellPolicy::Silent;

  [[nodiscard]] bool operator==(const TerminalProfile &) const = default;

  static constexpr int kMaxNameLength = 64;
  static constexpr int kMaxFontFamilyLength = 128;
  static constexpr int kMinFontSize = 6;
  static constexpr int kMaxFontSize = 48;
  static constexpr int kMaxScrollbackLines = 100000;
  static constexpr int kDefaultScrollbackLines = 10000;
  // User profiles are bounded; the built-in default does not count against
  // this limit.
  static constexpr int kMaxUserProfiles = 16;
};

struct ProfileValidation final {
  bool ok = false;
  QString diagnostic;

  [[nodiscard]] bool operator==(const ProfileValidation &) const = default;
};

// Structural validation only (no filesystem access): sizes, printable
// characters, identifier formats, and enum/range membership. Shell program
// existence/executability stays with the launch policy, which runs at
// session creation and fails closed with a typed diagnostic.
[[nodiscard]] ProfileValidation
validateTerminalProfile(const TerminalProfile &profile);

// The immutable built-in profile used before any Settings1 baseline and as
// the fallback whenever persisted data is missing or invalid.
[[nodiscard]] const TerminalProfile &builtinDefaultProfile();
[[nodiscard]] QString builtinDefaultProfileId();

// Fresh user-profile identity: 32 lowercase hex characters, unique in
// practice (UUID without braces). Caller still deduplicates against its
// own list.
[[nodiscard]] QString generateProfileId();

struct ProfileListCodecResult final {
  bool ok = false;
  QString diagnostic;
  QList<TerminalProfile> profiles;

  [[nodiscard]] bool operator==(const ProfileListCodecResult &) const =
      default;
};

// Canonical JSON codec for the Settings1 `terminal.profiles` value (a single
// string carrying a JSON array). Encode refuses more than
// TerminalProfile::kMaxUserProfiles entries. Decode is fail-closed: a
// structurally malformed document is rejected wholesale (ok=false, consumer
// keeps last confirmed state); a well-formed document drops individually
// invalid entries, never repairs them. Unknown fields are ignored so newer
// writers stay readable.
[[nodiscard]] QString
encodeTerminalProfiles(const QList<TerminalProfile> &profiles, bool *ok);
[[nodiscard]] ProfileListCodecResult
decodeTerminalProfiles(const QString &json);

} // namespace QindaQt::Apps::Terminal
