// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/apps/settings_startup/startup_settings_model.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>
#include <QtCore/QTextStream>


namespace QindaQt::Apps::SettingsStartup {
namespace {

constexpr auto EntryGroupHeader = "[Desktop Entry]";
constexpr auto CustomMarkerLine = "X-QindaQt-Custom=true";

bool parseSection(const QString &line, QString *section) {
  const QString trimmed = line.trimmed();
  if (!trimmed.startsWith(QLatin1Char('[')) ||
      !trimmed.endsWith(QLatin1Char(']'))) {
    return false;
  }
  *section = trimmed.mid(1, trimmed.size() - 2);
  return true;
}

// Reads exactly the keys this route understands from the [Desktop Entry]
// group of a raw .desktop file, ignoring locale-suffixed variants (Name[fr]
// etc.) and every other group -- this route neither reads nor writes
// anything outside its documented five keys.
struct DesktopEntryFields {
  QString name;
  QString comment;
  QString iconName;
  QString exec;
  bool hidden = false;
  bool gnomeAutostartDisabled = false;
  bool custom = false;
  bool valid = false;
};

DesktopEntryFields readDesktopEntry(const QString &path) {
  DesktopEntryFields fields;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return fields;
  }
  QTextStream stream(&file);
  QString section;
  bool sawType = false;
  while (!stream.atEnd()) {
    const QString rawLine = stream.readLine();
    QString parsedSection;
    if (parseSection(rawLine, &parsedSection)) {
      section = parsedSection;
      continue;
    }
    if (section != QLatin1String("Desktop Entry")) {
      continue;
    }
    const qsizetype equals = rawLine.indexOf(QLatin1Char('='));
    if (equals < 0) {
      continue;
    }
    const QString key = rawLine.left(equals).trimmed();
    const QString value = rawLine.mid(equals + 1).trimmed();
    if (key == QLatin1String("Type")) {
      sawType = value == QLatin1String("Application");
    } else if (key == QLatin1String("Name")) {
      fields.name = value;
    } else if (key == QLatin1String("Comment")) {
      fields.comment = value;
    } else if (key == QLatin1String("Icon")) {
      fields.iconName = value;
    } else if (key == QLatin1String("Exec")) {
      fields.exec = value;
    } else if (key == QLatin1String("Hidden")) {
      fields.hidden = value.compare(QLatin1String("true"),
                                    Qt::CaseInsensitive) == 0;
    } else if (key == QLatin1String("X-GNOME-Autostart-enabled")) {
      fields.gnomeAutostartDisabled =
          value.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0;
    } else if (key == QLatin1String("X-QindaQt-Custom")) {
      fields.custom =
          value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    }
  }
  // AGENT-GUARD: a file with no [Desktop Entry] group, or one that never
  // declared Type=Application, is not an autostart entry this route
  // presents -- fail closed rather than show a row for something that is
  // not actually launchable at login.
  fields.valid = sawType && !fields.name.trimmed().isEmpty();
  return fields;
}

QString slugify(const QString &name) {
  QString slug;
  slug.reserve(name.size());
  for (const QChar &character : name) {
    if (character.isLetterOrNumber()) {
      slug.append(character.toLower());
    } else if (!slug.isEmpty() && !slug.endsWith(QLatin1Char('-'))) {
      slug.append(QLatin1Char('-'));
    }
  }
  while (slug.endsWith(QLatin1Char('-'))) {
    slug.chop(1);
  }
  return slug.isEmpty() ? QStringLiteral("entry") : slug;
}

// Patch the two disable flags this route interprets. Re-enabling must
// clear both, including repeated keys; a Hidden=false override alone leaves
// GNOME's false flag disabling the same entry on the next login.
QString withEnabledSetTo(const QString &sourceText, bool enabled) {
  QStringList lines = sourceText.split(QLatin1Char('\n'));
  QString section;
  int entryHeader = -1;
  bool sawHidden = false;
  for (int index = 0; index < lines.size(); ++index) {
    QString parsed;
    if (parseSection(lines.at(index), &parsed)) {
      section = parsed;
      if (section == QLatin1String("Desktop Entry") && entryHeader < 0)
        entryHeader = index;
      continue;
    }
    if (section != QLatin1String("Desktop Entry"))
      continue;
    const QString key = lines.at(index).section(QLatin1Char('='), 0, 0).trimmed();
    if (key == QLatin1String("Hidden")) {
      lines[index] = enabled ? QStringLiteral("Hidden=false")
                             : QStringLiteral("Hidden=true");
      sawHidden = true;
    } else if (enabled && key == QLatin1String("X-GNOME-Autostart-enabled")) {
      lines[index] = QStringLiteral("X-GNOME-Autostart-enabled=true");
    }
  }
  if (!sawHidden) {
    const QString hidden = enabled ? QStringLiteral("Hidden=false")
                                   : QStringLiteral("Hidden=true");
    if (entryHeader >= 0)
      lines.insert(entryHeader + 1, hidden);
    else {
      lines.append(QStringLiteral("[Desktop Entry]"));
      lines.append(hidden);
    }
  }
  return lines.join(QLatin1Char('\n'));
}

bool writeFileAtomically(const QString &path, const QString &contents,
                         QString *error) {
  QSaveFile save(path);
  if (!save.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not open '%1' for writing").arg(path);
    }
    return false;
  }
  QTextStream stream(&save);
  stream.setEncoding(QStringConverter::Utf8);
  stream << contents;
  if (!save.commit()) {
    if (error != nullptr) {
      *error = QStringLiteral("could not persist '%1'").arg(path);
    }
    return false;
  }
  return true;
}

} // namespace

XdgAutostartStore::XdgAutostartStore()
    : m_options(SessionAutostart::ScanOptions::fromEnvironment()) {}

XdgAutostartStore::XdgAutostartStore(QString userDirectory,
                                     QStringList systemDirectories)
    : m_options(SessionAutostart::ScanOptions::fromEnvironment()) {
  m_options.userDirectory = std::move(userDirectory);
  m_options.systemDirectories = std::move(systemDirectories);
}

XdgAutostartStore::XdgAutostartStore(SessionAutostart::ScanOptions options)
    : m_options(std::move(options)) {}

QList<AutostartEntry> XdgAutostartStore::list(QString *error) {
  const QList<SessionAutostart::Entry> scanned =
      SessionAutostart::scan(m_options, error);
  QList<AutostartEntry> entries;
  entries.reserve(scanned.size());
  for (const auto &source : scanned) {
    AutostartEntry entry;
    entry.id = source.id;
    entry.name = source.name;
    entry.comment = source.comment;
    entry.iconName = source.iconName;
    entry.exec = source.exec;
    entry.enabled = source.enabled;
    entry.eligible = source.eligible;
    entry.ineligibilityReason = source.ineligibilityReason;
    entry.custom = source.custom;
    entries.append(std::move(entry));
  }
  return entries;
}

bool XdgAutostartStore::setEnabled(const QString &id, const bool enabled,
                                   QString *error) {
  if (id.isEmpty() || id == QLatin1String(".") || id == QLatin1String("..")
      || id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('\\'))) {
    if (error != nullptr)
      *error = QStringLiteral("invalid autostart entry id");
    return false;
  }
  const QString userPath =
      QDir(m_options.userDirectory).filePath(id + QStringLiteral(".desktop"));
  QString sourceText;
  if (QFile userFile(userPath); userFile.exists()) {
    if (!userFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      if (error != nullptr) {
        *error = QStringLiteral("could not read '%1'").arg(userPath);
      }
      return false;
    }
    sourceText = QString::fromUtf8(userFile.readAll());
  } else {
    // No user override yet: this id must come from a system directory, or
    // there is nothing to enable/disable.
    bool found = false;
    for (const QString &directory : m_options.systemDirectories) {
      const QString systemPath =
          QDir(directory).filePath(id + QStringLiteral(".desktop"));
      QFile systemFile(systemPath);
      if (!systemFile.exists()) {
        continue;
      }
      if (!systemFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        continue;
      }
      sourceText = QString::fromUtf8(systemFile.readAll());
      found = true;
      break;
    }
    if (!found) {
      if (error != nullptr) {
        *error = QStringLiteral("no autostart entry '%1' was found").arg(id);
      }
      return false;
    }
  }

  if (!QDir().mkpath(m_options.userDirectory)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not create '%1'").arg(m_options.userDirectory);
    }
    return false;
  }
  const QString patched = withEnabledSetTo(sourceText, enabled);
  return writeFileAtomically(userPath, patched, error);
}

QString XdgAutostartStore::addCommand(const QString &name,
                                      const QString &command,
                                      QString *error) {
  const QString trimmedName = name.trimmed();
  const QString trimmedCommand = command.trimmed();
  if (trimmedName.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("a name is required");
    }
    return {};
  }
  if (trimmedCommand.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("a command is required");
    }
    return {};
  }
  if (!QDir().mkpath(m_options.userDirectory)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not create '%1'").arg(m_options.userDirectory);
    }
    return {};
  }
  const QString baseId = QStringLiteral("qindaqt-custom-") + slugify(trimmedName);
  QString id = baseId;
  QDir dir(m_options.userDirectory);
  for (int suffix = 2; dir.exists(id + QStringLiteral(".desktop")); ++suffix) {
    id = baseId + QStringLiteral("-%1").arg(suffix);
  }
  // AGENT-GUARD: Exec is stored verbatim, never shell-quoted or interpreted
  // here; a value containing '\n' would corrupt the file's line structure,
  // so it is refused rather than silently truncated or escaped.
  if (trimmedName.contains(QLatin1Char('\n')) ||
      trimmedCommand.contains(QLatin1Char('\n'))) {
    if (error != nullptr) {
      *error = QStringLiteral("the name and command must be one line");
    }
    return {};
  }
  const QString contents = QStringLiteral(
      "%1\n"
      "Type=Application\n"
      "Name=%2\n"
      "Exec=%3\n"
      "%4\n"
      "Hidden=false\n")
      .arg(QLatin1String(EntryGroupHeader), trimmedName, trimmedCommand,
           QLatin1String(CustomMarkerLine));
  const QString path = dir.filePath(id + QStringLiteral(".desktop"));
  if (!writeFileAtomically(path, contents, error)) {
    return {};
  }
  return id;
}

bool XdgAutostartStore::removeCustom(const QString &id, QString *error) {
  if (id.isEmpty() || id.contains(QLatin1Char('/'))
      || id.contains(QLatin1Char('\\'))) {
    if (error != nullptr)
      *error = QStringLiteral("invalid autostart entry id");
    return false;
  }
  const QString path =
      QDir(m_options.userDirectory).filePath(id + QStringLiteral(".desktop"));
  const DesktopEntryFields fields = readDesktopEntry(path);
  // AGENT-GUARD: only an entry this route marked X-QindaQt-Custom=true may
  // be deleted outright. A real installed application's autostart file is
  // never removed by this route, only ever shadowed with Hidden=true.
  if (!fields.valid || !fields.custom) {
    if (error != nullptr) {
      *error = QStringLiteral("'%1' is not a custom entry this route can remove")
                   .arg(id);
    }
    return false;
  }
  if (!QFile::remove(path)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not remove '%1'").arg(path);
    }
    return false;
  }
  return true;
}

} // namespace QindaQt::Apps::SettingsStartup
