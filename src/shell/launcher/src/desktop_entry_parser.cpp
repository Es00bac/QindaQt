// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_launcher/desktop_entry_parser.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <optional>
#include <utility>

namespace QindaQt::ShellLauncher {
namespace {

struct RawAction {
  QString name;
  QString iconName;
};

DesktopEntryError makeError(DesktopEntryErrorCode code, int line, QString message)
{
  return DesktopEntryError { code, line, std::move(message) };
}

DesktopEntryParseResult failure(DesktopEntryErrorCode code, int line, QString message)
{
  return DesktopEntryParseResult { std::nullopt,
                                   makeError(code, line, std::move(message)) };
}

bool isKeyCharacter(QChar character)
{
  return character.isLetter() || character.isDigit()
      || character == QLatin1Char('-');
}

bool isActionId(const QString &id)
{
  if (id.isEmpty() || id.size() > Bounds::maxActionIdLength)
    return false;
  for (const QChar character : id) {
    const ushort value = character.unicode();
    const bool asciiLetter = (value >= 'A' && value <= 'Z')
        || (value >= 'a' && value <= 'z');
    if (!asciiLetter && !(value >= '0' && value <= '9')
        && value != '-')
      return false;
  }
  return true;
}

bool isEntryKey(const QString &key)
{
  static const QSet<QString> keys {
    QStringLiteral("Type"),       QStringLiteral("Name"),
    QStringLiteral("GenericName"), QStringLiteral("Comment"),
    QStringLiteral("Icon"),       QStringLiteral("NoDisplay"),
    QStringLiteral("Hidden"),     QStringLiteral("Categories"),
    QStringLiteral("Keywords"),   QStringLiteral("Actions"),
  };
  return keys.contains(key);
}

bool isActionKey(const QString &key)
{
  return key == QLatin1String("Name") || key == QLatin1String("Icon");
}

QString unescapeValue(const QString &value, int line, DesktopEntryError &error,
                      bool allowEscapedSemicolon = false)
{
  QString decoded;
  decoded.reserve(value.size());
  for (int index = 0; index < value.size(); ++index) {
    const QChar character = value.at(index);
    if (character != QLatin1Char('\\')) {
      decoded.append(character);
      continue;
    }
    if (index + 1 >= value.size()) {
      error = makeError(DesktopEntryErrorCode::InvalidEscape, line,
                        QStringLiteral("dangling escape at end of value"));
      return {};
    }
    const QChar escape = value.at(++index);
    switch (escape.unicode()) {
    case 's':
      decoded.append(QLatin1Char(' '));
      break;
    case 'n':
      decoded.append(QLatin1Char('\n'));
      break;
    case 't':
      decoded.append(QLatin1Char('\t'));
      break;
    case 'r':
      decoded.append(QLatin1Char('\r'));
      break;
    case '\\':
      decoded.append(QLatin1Char('\\'));
      break;
    case ';':
      if (allowEscapedSemicolon) {
        decoded.append(QLatin1Char(';'));
        break;
      }
      [[fallthrough]];
    default:
      error = makeError(DesktopEntryErrorCode::InvalidEscape, line,
                        QStringLiteral("unsupported escape sequence"));
      return {};
    }
  }
  return decoded;
}

std::optional<QStringList> parseListValue(const QString &value, int line,
                                          DesktopEntryError &error,
                                          bool rejectEmptyItems)
{
  QStringList items;
  QString item;
  item.reserve(value.size());
  for (int index = 0; index < value.size(); ++index) {
    if (value.at(index) == QLatin1Char(';')) {
      if (item.isEmpty()) {
        if (rejectEmptyItems) {
          error = makeError(DesktopEntryErrorCode::InvalidActionId, line,
                            QStringLiteral("action lists contain an empty id"));
          return std::nullopt;
        }
      } else {
        items.append(item);
      }
      item.clear();
      continue;
    }

    if (value.at(index) != QLatin1Char('\\')) {
      item.append(value.at(index));
      continue;
    }

    if (index + 1 >= value.size()) {
      error = makeError(DesktopEntryErrorCode::InvalidEscape, line,
                        QStringLiteral("dangling escape at end of list value"));
      return std::nullopt;
    }

    const QChar escape = value.at(index + 1);
    if (escape == QLatin1Char(';')) {
      item.append(QLatin1Char(';'));
      ++index;
      continue;
    }

    DesktopEntryError itemError;
    const QString decoded = unescapeValue(value.mid(index, 2), line, itemError,
                                          true);
    if (itemError.code != DesktopEntryErrorCode::None) {
      error = itemError;
      return std::nullopt;
    }
    item.append(decoded);
    ++index;
  }
  if (!item.isEmpty())
    items.append(item);
  return items;
}

std::optional<bool> parseBoolValue(const QString &value)
{
  if (value == QLatin1String("true"))
    return true;
  if (value == QLatin1String("false"))
    return false;
  return std::nullopt;
}

bool withinLimit(qsizetype length, qsizetype limit)
{
  return length >= 0 && length <= limit;
}

DesktopEntryError fieldError(const char *what)
{
  return makeError(DesktopEntryErrorCode::FieldLimitExceeded, 0,
                   QStringLiteral("%1 exceeds its ceiling")
                       .arg(QLatin1String(what)));
}

DesktopEntryError validateFieldLimits(const ParsedDesktopEntry &entry)
{
  if (!withinLimit(entry.name.size(), Bounds::maxNameLength))
    return fieldError("name");
  if (!withinLimit(entry.genericName.size(), Bounds::maxGenericNameLength))
    return fieldError("generic name");
  if (!withinLimit(entry.comment.size(), Bounds::maxCommentLength))
    return fieldError("comment");
  if (!withinLimit(entry.iconName.size(), Bounds::maxIconNameLength))
    return fieldError("icon");
  if (entry.categories.size() > Bounds::maxCategories)
    return fieldError("category count");
  for (const QString &category : entry.categories) {
    if (!withinLimit(category.size(), Bounds::maxCategoryLength))
      return fieldError("category");
  }
  if (entry.keywords.size() > Bounds::maxKeywords)
    return fieldError("keyword count");
  for (const QString &keyword : entry.keywords) {
    if (!withinLimit(keyword.size(), Bounds::maxKeywordLength))
      return fieldError("keyword");
  }
  if (entry.actions.size() > Bounds::maxActions)
    return fieldError("action count");
  for (const DesktopEntryAction &action : entry.actions) {
    if (!withinLimit(action.id.size(), Bounds::maxActionIdLength)
        || !withinLimit(action.name.size(), Bounds::maxActionNameLength)
        || !withinLimit(action.iconName.size(), Bounds::maxIconNameLength))
      return fieldError("action field");
  }
  return {};
}

DesktopEntryError appendDeclaredActions(
    const QStringList &declaredActionIds,
    const QHash<QString, RawAction> &rawActions,
    ParsedDesktopEntry &entry)
{
  QSet<QString> declaredIds;
  for (const QString &actionId : declaredActionIds) {
    if (!isActionId(actionId) || declaredIds.contains(actionId)) {
      return makeError(
          DesktopEntryErrorCode::InvalidActionId, 0,
          QStringLiteral("Actions= contains an empty, malformed, or duplicate id"));
    }
    declaredIds.insert(actionId);
    const auto actionIterator = rawActions.constFind(actionId);
    if (actionIterator == rawActions.constEnd()
        || actionIterator->name.trimmed().isEmpty()) {
      return makeError(
          DesktopEntryErrorCode::UnknownActionReference, 0,
          QStringLiteral("Actions= references unknown or unnamed action"));
    }
    entry.actions.append(
        DesktopEntryAction { actionId, actionIterator->name, actionIterator->iconName });
  }
  return {};
}

} // namespace

DesktopEntryParseResult DesktopEntryParser::parse(const QString &text)
{
  if (text.size() > Bounds::maxDocumentCodeUnits) {
    return failure(DesktopEntryErrorCode::DocumentTooLarge, 0,
                   QStringLiteral("document exceeds the %1 UTF-16 code-unit ceiling")
                       .arg(Bounds::maxDocumentCodeUnits));
  }

  bool seenEntryGroup = false;
  bool hidden = false;
  QString type;
  ParsedDesktopEntry entry;
  QHash<QString, RawAction> rawActions;
  QHash<QString, QSet<QString>> actionKeys;
  QSet<QString> entryKeys;
  QStringList declaredActionIds;

  const QStringList lines = text.split(QLatin1Char('\n'));
  QString currentGroup;
  QString currentActionId;

  for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
    QString line = lines.at(lineNumber);
    if (line.endsWith(QLatin1Char('\r')))
      line.chop(1);
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
      continue;

    if (trimmed.startsWith(QLatin1Char('['))
        && trimmed.endsWith(QLatin1Char(']'))) {
      const QString header = trimmed.mid(1, trimmed.size() - 2);
      currentActionId.clear();
      if (header == QLatin1String("Desktop Entry")) {
        if (seenEntryGroup) {
          return failure(DesktopEntryErrorCode::DuplicateEntryGroup,
                         lineNumber + 1,
                         QStringLiteral("duplicate [Desktop Entry] group"));
        }
        seenEntryGroup = true;
        currentGroup = header;
      } else if (header.startsWith(QLatin1String("Desktop Action "))) {
        currentActionId = header.mid(15);
        if (!isActionId(currentActionId)) {
          return failure(DesktopEntryErrorCode::InvalidActionId,
                         lineNumber + 1,
                         QStringLiteral("desktop action id is empty or malformed"));
        }
        if (rawActions.contains(currentActionId)) {
          return failure(DesktopEntryErrorCode::DuplicateActionGroup,
                         lineNumber + 1,
                         QStringLiteral("duplicate desktop action group"));
        }
        currentGroup = header;
        rawActions.insert(currentActionId, RawAction {});
      } else {
        currentGroup = header;
      }
      continue;
    }

    if (currentGroup.isEmpty()) {
      return failure(DesktopEntryErrorCode::InvalidKeyLine, lineNumber + 1,
                     QStringLiteral("key outside of any group"));
    }

    const qsizetype separator = line.indexOf(QLatin1Char('='));
    if (separator <= 0) {
      return failure(DesktopEntryErrorCode::InvalidKeyLine, lineNumber + 1,
                     QStringLiteral("line is neither a group header nor Key=Value"));
    }

    const QString key = line.left(separator).trimmed();
    if (key.contains(QLatin1Char('['))) {
      // Locale-suffixed variants are owned by a later locale-selection
      // boundary. Their payload is intentionally not decoded here.
      continue;
    }
    for (const QChar character : key) {
      if (!isKeyCharacter(character)) {
        return failure(DesktopEntryErrorCode::InvalidKeyLine, lineNumber + 1,
                       QStringLiteral("key contains an invalid character"));
      }
    }

    const bool inEntryGroup = currentGroup == QLatin1String("Desktop Entry");
    const bool owned = (inEntryGroup && isEntryKey(key))
        || (!currentActionId.isEmpty() && isActionKey(key));
    // AGENT-GUARD: Unknown extension payloads are never decoded. This keeps an
    // escape syntax owned by another specification extension from rejecting a
    // document that this bounded model can otherwise consume.
    if (!owned)
      continue;

    QSet<QString> &seenKeys = inEntryGroup ? entryKeys : actionKeys[currentActionId];
    if (seenKeys.contains(key)) {
      return failure(DesktopEntryErrorCode::DuplicateKey, lineNumber + 1,
                     QStringLiteral("duplicate recognized key in one group"));
    }
    seenKeys.insert(key);

    const QString encodedValue = line.mid(separator + 1).trimmed();
    DesktopEntryError valueError;
    QString value;
    if (key == QLatin1String("Categories") || key == QLatin1String("Keywords")
        || key == QLatin1String("Actions")) {
      const auto values = parseListValue(encodedValue, lineNumber + 1, valueError,
                                         key == QLatin1String("Actions"));
      if (!values)
        return DesktopEntryParseResult { std::nullopt, valueError };
      if (key == QLatin1String("Categories"))
        entry.categories = *values;
      else if (key == QLatin1String("Keywords"))
        entry.keywords = *values;
      else
        declaredActionIds = *values;
      continue;
    }

    value = unescapeValue(encodedValue, lineNumber + 1, valueError);
    if (valueError.code != DesktopEntryErrorCode::None)
      return DesktopEntryParseResult { std::nullopt, valueError };

    if (inEntryGroup) {
      if (key == QLatin1String("Type"))
        type = value;
      else if (key == QLatin1String("Name"))
        entry.name = value;
      else if (key == QLatin1String("GenericName"))
        entry.genericName = value;
      else if (key == QLatin1String("Comment"))
        entry.comment = value;
      else if (key == QLatin1String("Icon"))
        entry.iconName = value;
      else if (key == QLatin1String("NoDisplay")
               || key == QLatin1String("Hidden")) {
        const auto flag = parseBoolValue(value);
        if (!flag) {
          return failure(DesktopEntryErrorCode::InvalidBoolean, lineNumber + 1,
                         QStringLiteral("boolean values must be true or false"));
        }
        hidden = hidden || *flag;
      }
    } else if (key == QLatin1String("Name")) {
      rawActions[currentActionId].name = value;
    } else if (key == QLatin1String("Icon")) {
      rawActions[currentActionId].iconName = value;
    }
  }

  if (!seenEntryGroup) {
    return failure(DesktopEntryErrorCode::MissingEntryGroup, 0,
                   QStringLiteral("no [Desktop Entry] group found"));
  }
  if (type != QLatin1String("Application")) {
    return failure(DesktopEntryErrorCode::UnsupportedType, 0,
                   QStringLiteral("only Type=Application entries are launchable"));
  }
  if (entry.name.trimmed().isEmpty()) {
    return failure(DesktopEntryErrorCode::MissingName, 0,
                   QStringLiteral("entry has no non-blank Name"));
  }

  if (const DesktopEntryError actionError =
          appendDeclaredActions(declaredActionIds, rawActions, entry);
      actionError.code != DesktopEntryErrorCode::None)
    return DesktopEntryParseResult { std::nullopt, actionError };

  if (const DesktopEntryError limitError = validateFieldLimits(entry);
      limitError.code != DesktopEntryErrorCode::None)
    return DesktopEntryParseResult { std::nullopt, limitError };

  entry.hidden = hidden;
  return DesktopEntryParseResult { entry, {} };
}

} // namespace QindaQt::ShellLauncher
