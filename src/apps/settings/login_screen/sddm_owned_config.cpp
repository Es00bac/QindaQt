// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_login_screen/sddm_owned_config.h>

#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QSaveFile>
#include <QtCore/QSet>

namespace QindaQt::Apps::SettingsLoginScreen {
namespace {

// One owned key's place in the INI shape, in a fixed order so serialized
// payloads and appended sections are deterministic for tests and diffs.
struct OwnedKeySpec {
  QString section;
  QString key;
};

const OwnedKeySpec kThemeSpec{QStringLiteral("Theme"),
                              QStringLiteral("Current")};
const OwnedKeySpec kNumlockSpec{QStringLiteral("General"),
                                QStringLiteral("Numlock")};
const OwnedKeySpec kCursorSpec{QStringLiteral("Theme"),
                               QStringLiteral("CursorTheme")};
const OwnedKeySpec kUserSpec{QStringLiteral("Autologin"),
                             QStringLiteral("User")};
const OwnedKeySpec kSessionSpec{QStringLiteral("Autologin"),
                                QStringLiteral("Session")};

[[nodiscard]] QString tagOf(const OwnedKeySpec &spec) {
  return spec.section + u'/' + spec.key;
}

[[nodiscard]] QList<std::pair<OwnedKeySpec, QString>>
ownedEntries(const SddmOwnedChangeSet &changes) {
  QList<std::pair<OwnedKeySpec, QString>> entries;
  if (changes.theme.has_value()) {
    entries.append({kThemeSpec, *changes.theme});
  }
  if (changes.numlock.has_value()) {
    entries.append({kNumlockSpec, *changes.numlock});
  }
  if (changes.cursorTheme.has_value()) {
    entries.append({kCursorSpec, *changes.cursorTheme});
  }
  if (changes.autologinUser.has_value()) {
    entries.append({kUserSpec, *changes.autologinUser});
  }
  if (changes.autologinSession.has_value()) {
    entries.append({kSessionSpec, *changes.autologinSession});
  }
  return entries;
}

// AGENT-GUARD: a value must never inject a line break or NUL into an INI
// file -- that is how one key becomes two and a config file becomes a
// script delivery mechanism. Both the client and the helper run this check.
[[nodiscard]] bool isSafeValue(const QString &value) noexcept {
  return !value.contains(QChar::Null) && !value.contains(u'\n') &&
         !value.contains(u'\r') && value.size() <= 256;
}

} // namespace

bool isValidSddmNumlockValue(const QString &value) noexcept {
  return value == QLatin1String("on") || value == QLatin1String("off") ||
         value == QLatin1String("none");
}

QString serializeOwnedChangeSet(const SddmOwnedChangeSet &changes) {
  QString text;
  QString openSection;
  for (const auto &[spec, value] : ownedEntries(changes)) {
    if (openSection != spec.section) {
      if (!openSection.isEmpty()) {
        text += u'\n';
      }
      text += u'[' + spec.section + QStringLiteral("]\n");
      openSection = spec.section;
    }
    text += spec.key + u'=' + value + u'\n';
  }
  return text;
}

std::optional<SddmOwnedChangeSet>
deserializeOwnedChangeSet(const QString &text, QString *error) {
  SddmOwnedChangeSet changes;
  QString section;
  const QStringList lines = text.split(u'\n');
  for (const QString &rawLine : lines) {
    const QString line = rawLine.trimmed();
    if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';')) {
      continue;
    }
    if (line.startsWith(u'[') && line.endsWith(u']')) {
      section = line.mid(1, line.size() - 2).trimmed();
      if (section != kThemeSpec.section && section != kNumlockSpec.section &&
          section != kUserSpec.section) {
        if (error != nullptr) {
          *error = QStringLiteral("payload names a section this route does "
                                  "not own: [%1]")
                       .arg(section);
        }
        return std::nullopt;
      }
      continue;
    }
    const qsizetype equals = line.indexOf(u'=');
    if (equals <= 0 || section.isEmpty()) {
      if (error != nullptr) {
        *error = QStringLiteral("malformed payload line: '%1'").arg(line);
      }
      return std::nullopt;
    }
    const QString key = line.left(equals).trimmed();
    const QString value = line.mid(equals + 1).trimmed();
    if (!isSafeValue(value)) {
      if (error != nullptr) {
        *error = QStringLiteral("unsafe value for %1: line breaks, NUL or "
                                "over 256 characters are not allowed")
                     .arg(key);
      }
      return std::nullopt;
    }
    if (section == kThemeSpec.section && key == kThemeSpec.key) {
      changes.theme = value;
    } else if (section == kNumlockSpec.section && key == kNumlockSpec.key) {
      changes.numlock = value;
    } else if (section == kCursorSpec.section && key == kCursorSpec.key) {
      changes.cursorTheme = value;
    } else if (section == kUserSpec.section && key == kUserSpec.key) {
      changes.autologinUser = value;
    } else if (section == kSessionSpec.section && key == kSessionSpec.key) {
      changes.autologinSession = value;
    } else {
      if (error != nullptr) {
        *error = QStringLiteral("payload names a key this route does not "
                                "own: %1/%2")
                     .arg(section, key);
      }
      return std::nullopt;
    }
  }
  if (changes.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("payload carries no change");
    }
    return std::nullopt;
  }
  return changes;
}

QString validateOwnedChangeSet(const SddmOwnedChangeSet &changes,
                               const QStringList &installedThemeIds,
                               const QStringList &installedSessionIds,
                               const QStringList &loginUserNames) {
  if (changes.theme.has_value()) {
    if (!isSafeValue(*changes.theme)) {
      return QStringLiteral("Theme value is not usable text.");
    }
    if (!changes.theme->isEmpty() &&
        !installedThemeIds.contains(*changes.theme)) {
      return QStringLiteral("The theme '%1' is not installed.")
          .arg(*changes.theme);
    }
  }
  if (changes.numlock.has_value() &&
      !isValidSddmNumlockValue(*changes.numlock)) {
    return QStringLiteral("Numeric lock must be on, off or none, not '%1'.")
        .arg(*changes.numlock);
  }
  if (changes.cursorTheme.has_value() && !isSafeValue(*changes.cursorTheme)) {
    return QStringLiteral("Cursor theme value is not usable text.");
  }
  if (changes.autologinUser.has_value() && !changes.autologinUser->isEmpty() &&
      !loginUserNames.contains(*changes.autologinUser)) {
    return QStringLiteral("The user '%1' does not exist.")
        .arg(*changes.autologinUser);
  }
  if (changes.autologinSession.has_value() &&
      !changes.autologinSession->isEmpty() &&
      !installedSessionIds.contains(*changes.autologinSession)) {
    return QStringLiteral("The session '%1' does not exist.")
        .arg(*changes.autologinSession);
  }
  return QString();
}

QString mergeOwnedChangeSetIntoConfigText(const QString &existingText,
                                          const SddmOwnedChangeSet &changes) {
  const auto entries = ownedEntries(changes);
  QStringList lines = existingText.split(u'\n');
  // A trailing newline leaves one empty tail element; remember it so the
  // file neither gains nor loses its final newline.
  bool trailingNewline = false;
  if (!lines.isEmpty() && lines.constLast().isEmpty()) {
    trailingNewline = true;
    lines.removeLast();
  }

  // Pass 1: for every owned key in this change, remember the LAST line
  // carrying it. SDDM reads later values as winning, so the last duplicate
  // is the one that must be patched for the write to take effect.
  QHash<QString, qsizetype> lastLineByTag;
  QString section;
  for (qsizetype i = 0; i < lines.size(); ++i) {
    const QString trimmed = lines.at(i).trimmed();
    if (trimmed.startsWith(u'[') && trimmed.endsWith(u']')) {
      section = trimmed.mid(1, trimmed.size() - 2).trimmed();
      continue;
    }
    const qsizetype equals = trimmed.indexOf(u'=');
    if (equals <= 0) {
      continue;
    }
    const QString key = trimmed.left(equals).trimmed();
    for (const auto &entry : entries) {
      const OwnedKeySpec &spec = entry.first;
      if (section == spec.section && key == spec.key) {
        lastLineByTag.insert(tagOf(spec), i);
      }
    }
  }

  // Pass 2: rebuild, patching the winning lines and dropping nothing else.
  QSet<QString> written;
  QStringList out;
  out.reserve(lines.size() + static_cast<int>(entries.size()) + 4);
  for (qsizetype i = 0; i < lines.size(); ++i) {
    bool patched = false;
    for (const auto &[spec, value] : entries) {
      const QString tag = tagOf(spec);
      if (lastLineByTag.value(tag, -1) == i && !written.contains(tag)) {
        out.append(spec.key + u'=' + value);
        written.insert(tag);
        patched = true;
        break;
      }
    }
    if (!patched) {
      out.append(lines.at(i));
    }
  }

  // Pass 3: keys with no existing line join their section. When the file
  // already has the section, the key goes at the end of that section block
  // (just before the next header); otherwise a fresh section is appended.
  for (const auto &[spec, value] : entries) {
    const QString tag = tagOf(spec);
    if (written.contains(tag)) {
      continue;
    }
    const QString header = u'[' + spec.section + u']';
    qsizetype sectionStart = -1;
    for (qsizetype i = 0; i < out.size(); ++i) {
      if (out.at(i).trimmed() == header) {
        sectionStart = i;
        break;
      }
    }
    if (sectionStart < 0) {
      if (!out.isEmpty() && !out.constLast().trimmed().isEmpty()) {
        out.append(QString());
      }
      out.append(header);
      out.append(spec.key + u'=' + value);
    } else {
      qsizetype insertAt = out.size();
      for (qsizetype i = sectionStart + 1; i < out.size(); ++i) {
        const QString t = out.at(i).trimmed();
        if (t.startsWith(u'[') && t.endsWith(u']')) {
          insertAt = i;
          break;
        }
      }
      out.insert(insertAt, spec.key + u'=' + value);
    }
    written.insert(tag);
  }

  QString result = out.join(u'\n');
  if ((trailingNewline || !result.isEmpty()) && !result.endsWith(u'\n')) {
    result += u'\n';
  }
  return result;
}

bool writeOwnedChangeSetToFile(const QString &targetPath,
                               const SddmOwnedChangeSet &changes,
                               QString *error) {
  QString existing;
  QFile input(targetPath);
  if (input.exists()) {
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
      if (error != nullptr) {
        *error = QStringLiteral("Cannot read %1: %2")
                      .arg(targetPath, input.errorString());
      }
      return false;
    }
    existing = QString::fromUtf8(input.readAll());
    input.close();
  }

  const QString merged = mergeOwnedChangeSetIntoConfigText(existing, changes);
  QSaveFile output(targetPath);
  if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (error != nullptr) {
      *error = QStringLiteral("Cannot write %1: %2")
                    .arg(targetPath, output.errorString());
    }
    return false;
  }
  output.write(merged.toUtf8());
  if (!output.commit()) {
    if (error != nullptr) {
      *error = QStringLiteral("Cannot replace %1: %2")
                    .arg(targetPath, output.errorString());
    }
    return false;
  }
  // AGENT-GUARD: SDDM reads its configuration as root before any user logs
  // in; the drop-in must stay world-readable and never become executable,
  // whatever the caller's umask said.
  QFile::setPermissions(targetPath,
                        QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                            QFileDevice::ReadGroup | QFileDevice::ReadOther);
  return true;
}

} // namespace QindaQt::Apps::SettingsLoginScreen
