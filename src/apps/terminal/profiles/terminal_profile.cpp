// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiles/terminal_profile.h"

#include "session/terminal_launch_policy.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <cmath>
#include <optional>

namespace QindaQt::Apps::Terminal {
namespace {

// Identifier formats follow the CLI safe-id rule: lowercase, digit/hyphen,
// bounded.
const QRegularExpression &idPattern() {
  static const QRegularExpression pattern(
      QStringLiteral("^[a-z0-9][a-z0-9-]{0,31}$"));
  return pattern;
}

bool isPrintableText(const QString &text) {
  for (qsizetype index = 0; index < text.size(); ++index) {
    const QChar character = text.at(index);
    if (character.isHighSurrogate()) {
      if (index + 1 >= text.size() || !text.at(index + 1).isLowSurrogate()) {
        return false;
      }
      ++index;
      continue;
    }
    if (character.isLowSurrogate() || !character.isPrint()) {
      return false;
    }
  }
  return true;
}

bool hasControlCharacters(const QString &text) {
  for (const QChar character : text) {
    if (character.category() == QChar::Other_Control) {
      return true;
    }
  }
  return false;
}

ProfileValidation fail(const QString &diagnostic) {
  return {.ok = false, .diagnostic = diagnostic};
}

ProfileValidation validateShellArguments(const QStringList &arguments) {
  if (arguments.size() > TerminalLaunchPolicy::kMaxArguments) {
    return fail(QStringLiteral("Shell argument list exceeds the 64-entry "
                               "bound"));
  }
  for (const QString &argument : arguments) {
    if (argument.toUtf8().size() > TerminalLaunchPolicy::kMaxArgumentLength) {
      return fail(QStringLiteral("A shell argument exceeds the 4096-character "
                                 "bound"));
    }
    if (hasControlCharacters(argument)) {
      return fail(QStringLiteral("Shell arguments must not contain control "
                                 "characters"));
    }
  }
  return {.ok = true, .diagnostic = {}};
}

QJsonObject profileToJson(const TerminalProfile &profile) {
  QJsonObject object;
  object.insert(QStringLiteral("id"), profile.id);
  object.insert(QStringLiteral("name"), profile.name);
  object.insert(QStringLiteral("shellProgram"), profile.shellProgram);
  object.insert(QStringLiteral("shellArguments"),
                QJsonArray::fromStringList(profile.shellArguments));
  object.insert(QStringLiteral("fontFamily"), profile.fontFamily);
  object.insert(QStringLiteral("fontSize"), profile.fontSize);
  object.insert(QStringLiteral("colorSchemeId"), profile.colorSchemeId);
  object.insert(QStringLiteral("scrollbackLines"), profile.scrollbackLines);
  object.insert(QStringLiteral("bellPolicy"),
                profile.bellPolicy == TerminalProfile::BellPolicy::Audible
                    ? QStringLiteral("audible")
                    : QStringLiteral("silent"));
  return object;
}

// Returns std::nullopt when the entry is invalid; decode drops those
// entries (fail-closed, never repaired).
std::optional<TerminalProfile> profileFromJson(const QJsonValue &value) {
  if (!value.isObject()) {
    return std::nullopt;
  }
  const QJsonObject object = value.toObject();
  TerminalProfile profile;
  profile.id = object.value(QStringLiteral("id")).toString();
  profile.name = object.value(QStringLiteral("name")).toString();
  profile.shellProgram =
      object.value(QStringLiteral("shellProgram")).toString();
  const QJsonValue arguments = object.value(QStringLiteral("shellArguments"));
  if (arguments.isArray()) {
    const QJsonArray array = arguments.toArray();
    for (const QJsonValue &entry : array) {
      if (!entry.isString()) {
        return std::nullopt;
      }
      profile.shellArguments.append(entry.toString());
    }
  } else if (!arguments.isUndefined()) {
    return std::nullopt;
  }
  profile.fontFamily = object.value(QStringLiteral("fontFamily")).toString();
  const QJsonValue fontSize = object.value(QStringLiteral("fontSize"));
  if (fontSize.isDouble()) {
    // JSON numbers decode as double; reject fractional values instead of
    // silently truncating hostile input.
    const double size = fontSize.toDouble();
    if (size < 0.0 || size > TerminalProfile::kMaxFontSize ||
        size != std::floor(size)) {
      return std::nullopt;
    }
    profile.fontSize = static_cast<int>(size);
  } else if (!fontSize.isUndefined()) {
    return std::nullopt;
  }
  profile.colorSchemeId =
      object.value(QStringLiteral("colorSchemeId")).toString();
  // Canonicalize legacy QST theme ids at the persistence boundary so the
  // in-memory profile always carries a canonical content-scheme id.
  if (!isTerminalContentSchemeId(profile.colorSchemeId)) {
    if (const auto mapped =
            terminalContentSchemeForId(profile.colorSchemeId)) {
      profile.colorSchemeId = terminalContentSchemeId(*mapped);
    }
  }
  const QJsonValue scrollback = object.value(QStringLiteral("scrollbackLines"));
  if (scrollback.isDouble()) {
    const double lines = scrollback.toDouble();
    if (lines < 0.0 || lines > TerminalProfile::kMaxScrollbackLines ||
        lines != std::floor(lines)) {
      return std::nullopt;
    }
    profile.scrollbackLines = static_cast<int>(lines);
  } else if (!scrollback.isUndefined()) {
    return std::nullopt;
  }
  const QString bell = object.value(QStringLiteral("bellPolicy"))
                           .toString(QStringLiteral("silent"));
  if (bell == QLatin1String("audible")) {
    profile.bellPolicy = TerminalProfile::BellPolicy::Audible;
  } else if (bell == QLatin1String("silent")) {
    profile.bellPolicy = TerminalProfile::BellPolicy::Silent;
  } else {
    return std::nullopt;
  }
  if (!validateTerminalProfile(profile).ok) {
    return std::nullopt;
  }
  return profile;
}

} // namespace

QString terminalContentSchemeId(TerminalContentScheme scheme) {
  switch (scheme) {
  case TerminalContentScheme::Light:
    return QStringLiteral("light");
  case TerminalContentScheme::Dark:
    return QStringLiteral("dark");
  case TerminalContentScheme::System:
    break;
  }
  return QStringLiteral("system");
}

bool isTerminalContentSchemeId(const QString &id) {
  return id == QLatin1String("system") || id == QLatin1String("light") ||
         id == QLatin1String("dark");
}

std::optional<TerminalContentScheme>
terminalContentSchemeForId(const QString &id) {
  if (id == QLatin1String("system")) {
    return TerminalContentScheme::System;
  }
  if (id == QLatin1String("light")) {
    return TerminalContentScheme::Light;
  }
  if (id == QLatin1String("dark")) {
    return TerminalContentScheme::Dark;
  }
  // AGENT-NOTE: Legacy QST theme ids persist in pre-ADR-0116 profiles. Map by
  // the retired theme's variant so those profiles keep a recognizable surface
  // instead of failing the whole profile list decode.
  if (id == QLatin1String("qinda-dark") || id == QLatin1String("qinda-dusk") ||
      id == QLatin1String("qinda-high-contrast")) {
    return TerminalContentScheme::Dark;
  }
  if (id == QLatin1String("qinda-light") ||
      id == QLatin1String("qinda-macos")) {
    return TerminalContentScheme::Light;
  }
  return std::nullopt;
}

ProfileValidation validateTerminalProfile(const TerminalProfile &profile) {
  if (profile.id.isEmpty() || !idPattern().match(profile.id).hasMatch()) {
    return fail(QStringLiteral("Profile identifier has an invalid format"));
  }
  const QString name = profile.name.trimmed();
  if (name.isEmpty() || name != profile.name) {
    return fail(QStringLiteral("Profile name must not be empty or padded"));
  }
  if (name.size() > TerminalProfile::kMaxNameLength) {
    return fail(QStringLiteral("Profile name exceeds the %1-character bound")
                    .arg(TerminalProfile::kMaxNameLength));
  }
  if (!isPrintableText(name)) {
    return fail(
        QStringLiteral("Profile name must contain printable characters"));
  }
  if (!profile.shellProgram.isEmpty()) {
    if (profile.shellProgram.toUtf8().size() >
        TerminalLaunchPolicy::kMaxProgramLength) {
      return fail(QStringLiteral("Shell program path exceeds the "
                                 "4096-character bound"));
    }
    if (hasControlCharacters(profile.shellProgram)) {
      return fail(QStringLiteral("Shell program path must not contain "
                                 "control characters"));
    }
    if (!QDir::isAbsolutePath(profile.shellProgram)) {
      return fail(QStringLiteral("Shell program must be an absolute path"));
    }
  }
  const ProfileValidation arguments =
      validateShellArguments(profile.shellArguments);
  if (!arguments.ok) {
    return arguments;
  }
  if (profile.shellProgram.isEmpty() && !profile.shellArguments.isEmpty()) {
    return fail(QStringLiteral("Shell arguments require an explicit shell "
                               "program"));
  }
  if (!profile.fontFamily.isEmpty()) {
    if (profile.fontFamily.size() > TerminalProfile::kMaxFontFamilyLength) {
      return fail(QStringLiteral("Font family exceeds the %1-character bound")
                      .arg(TerminalProfile::kMaxFontFamilyLength));
    }
    if (profile.fontFamily != profile.fontFamily.trimmed() ||
        !isPrintableText(profile.fontFamily)) {
      return fail(QStringLiteral("Font family must be trimmed printable "
                                 "text"));
    }
  }
  if (profile.fontSize != 0 &&
      (profile.fontSize < TerminalProfile::kMinFontSize ||
       profile.fontSize > TerminalProfile::kMaxFontSize)) {
    return fail(QStringLiteral("Font size must be 0 (theme default) or "
                               "%1..%2")
                    .arg(TerminalProfile::kMinFontSize)
                    .arg(TerminalProfile::kMaxFontSize));
  }
  if (!isTerminalContentSchemeId(profile.colorSchemeId)) {
    return fail(QStringLiteral("Color scheme identifier is not a known "
                               "terminal content scheme"));
  }
  if (profile.scrollbackLines < 0 ||
      profile.scrollbackLines > TerminalProfile::kMaxScrollbackLines) {
    return fail(QStringLiteral("Scrollback must be between 0 and %1 lines")
                    .arg(TerminalProfile::kMaxScrollbackLines));
  }
  if (profile.bellPolicy != TerminalProfile::BellPolicy::Silent &&
      profile.bellPolicy != TerminalProfile::BellPolicy::Audible) {
    return fail(QStringLiteral("Bell policy is not recognized"));
  }
  return {.ok = true, .diagnostic = {}};
}

ProfileValidation
validateTerminalProfileList(const QList<TerminalProfile> &profiles) {
  if (profiles.size() > TerminalProfile::kMaxUserProfiles) {
    return fail(QStringLiteral("Profile list exceeds the %1-entry bound")
                    .arg(TerminalProfile::kMaxUserProfiles));
  }
  QSet<QString> identities;
  for (const TerminalProfile &profile : profiles) {
    const ProfileValidation validation = validateTerminalProfile(profile);
    if (!validation.ok) {
      return validation;
    }
    if (profile.id == builtinDefaultProfileId()) {
      return fail(QStringLiteral("A user profile cannot use the built-in "
                                 "profile identifier"));
    }
    if (identities.contains(profile.id)) {
      return fail(QStringLiteral("Profile identifiers must be unique"));
    }
    identities.insert(profile.id);
  }
  return {.ok = true, .diagnostic = {}};
}

const TerminalProfile &builtinDefaultProfile() {
  static const TerminalProfile profile{
      .id = QStringLiteral("builtin-default"),
      .name = QStringLiteral("Default"),
      .shellProgram = {},
      .shellArguments = {},
      .fontFamily = {},
      .fontSize = 0,
      .colorSchemeId = QStringLiteral("system"),
      .scrollbackLines = TerminalProfile::kDefaultScrollbackLines,
      .bellPolicy = TerminalProfile::BellPolicy::Silent,
  };
  return profile;
}

QString builtinDefaultProfileId() { return builtinDefaultProfile().id; }

QString generateProfileId() {
  QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  id.remove(QLatin1Char('-'));
  return id;
}

QString encodeTerminalProfiles(const QList<TerminalProfile> &profiles,
                               bool *ok) {
  if (ok != nullptr) {
    *ok = false;
  }
  if (!validateTerminalProfileList(profiles).ok) {
    return {};
  }
  QJsonArray array;
  for (const TerminalProfile &profile : profiles) {
    array.append(profileToJson(profile));
  }
  if (ok != nullptr) {
    *ok = true;
  }
  return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

ProfileListCodecResult decodeTerminalProfiles(const QString &json) {
  QJsonParseError parseError{};
  const QJsonDocument document =
      QJsonDocument::fromJson(json.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Profile document is not valid "
                                         "JSON array"),
            .profiles = {}};
  }
  const QJsonArray array = document.array();
  if (array.size() > TerminalProfile::kMaxUserProfiles) {
    return {.ok = false,
            .diagnostic = QStringLiteral("Profile document exceeds the "
                                         "bounded profile list"),
            .profiles = {}};
  }
  ProfileListCodecResult result{.ok = true, .diagnostic = {}, .profiles = {}};
  for (const QJsonValue &value : array) {
    const auto profile = profileFromJson(value);
    if (!profile.has_value()) {
      return {.ok = false,
              .diagnostic = QStringLiteral("Profile document contains an "
                                           "invalid entry"),
              .profiles = {}};
    }
    result.profiles.append(*profile);
  }
  const ProfileValidation validation =
      validateTerminalProfileList(result.profiles);
  if (!validation.ok) {
    return {.ok = false, .diagnostic = validation.diagnostic, .profiles = {}};
  }
  return result;
}

} // namespace QindaQt::Apps::Terminal
