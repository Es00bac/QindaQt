// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

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
  // Empty selects the platform theme's monospace font; otherwise an
  // installed font family name (QFont performs runtime matching).
  QString fontFamily;
  // 0 selects the platform monospace font's size; otherwise kMinFontSize..
  // kMaxFontSize.
  int fontSize = 0;
  // Terminal content scheme identifier ("system", "light", "dark") selecting
  // the ANSI protocol palette and content surface (ADR-0112/ADR-0116). Window
  // chrome never follows it. Default "system".
  QString colorSchemeId;
  // Bounded scrollback in lines, 0..kMaxScrollbackLines.
  int scrollbackLines = kDefaultScrollbackLines;

  enum class BellPolicy { Silent, Audible };
  // Silent ignores parsed bell notifications; Audible requests a GUI beep.
  // Both preserve PTY bytes, including BEL used to terminate OSC sequences.
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

// AGENT-CONTRACT: Terminal content schemes are the profile's terminal color
// scheme selection (ADR-0112/ADR-0116). They select ANSI protocol palettes and
// the content surface only; window chrome always follows the Qt platform
// theme and Fusion. The persisted ids ("system"/"light"/"dark") are the
// profile codec vocabulary.
enum class TerminalContentScheme { System, Light, Dark };

[[nodiscard]] QString terminalContentSchemeId(TerminalContentScheme scheme);
// True only for the canonical persisted ids ("system"/"light"/"dark"); legacy
// QST ids are NOT admitted here — decode maps them through
// terminalContentSchemeForId before validation runs.
[[nodiscard]] bool isTerminalContentSchemeId(const QString &id);
// Resolves a persisted id. Legacy QST theme ids map by variant
// (qinda-dark/qinda-dusk/qinda-high-contrast -> dark,
// qinda-light/qinda-macos -> light) so profiles written before ADR-0116 stay
// loadable; anything else returns nullopt and callers fail closed.
[[nodiscard]] std::optional<TerminalContentScheme>
terminalContentSchemeForId(const QString &id);

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

// Validates one complete user list atomically: every entry is valid, ids are
// unique, no entry impersonates the immutable built-in profile, and the list
// stays within kMaxUserProfiles. Callers must use this before publishing or
// encoding a list; accepting only a valid prefix would make hostile Settings1
// data look authoritative.
[[nodiscard]] ProfileValidation
validateTerminalProfileList(const QList<TerminalProfile> &profiles);

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

  [[nodiscard]] bool operator==(const ProfileListCodecResult &) const = default;
};

// Canonical JSON codec for the Settings1 `services.terminalProfiles` value (a
// single string carrying a JSON array). Encode refuses more than
// TerminalProfile::kMaxUserProfiles entries. Decode is fail-closed: a
// structurally malformed document or invalid/duplicate entry is rejected
// wholesale (ok=false, consumer applies built-in defaults). Unknown fields
// are ignored so newer writers stay readable.
[[nodiscard]] QString
encodeTerminalProfiles(const QList<TerminalProfile> &profiles, bool *ok);
[[nodiscard]] ProfileListCodecResult
decodeTerminalProfiles(const QString &json);

} // namespace QindaQt::Apps::Terminal
