// SPDX-License-Identifier: GPL-3.0-or-later
#include "links/terminal_link.h"

#include <QDir>
#include <QUrl>

namespace QindaQt::Apps::Terminal {
namespace {

[[nodiscard]] bool isBoundary(QChar character) {
  return character.isSpace() || character == QLatin1Char('(') ||
         character == QLatin1Char('[') || character == QLatin1Char('{') ||
         character == QLatin1Char('<') || character == QLatin1Char('"') ||
         character == QLatin1Char('\'');
}

[[nodiscard]] QString stripControls(const QString &input) {
  QString output;
  output.reserve(input.size());
  for (qsizetype index = 0; index < input.size(); ++index) {
    const QChar character = input.at(index);
    if (character.isHighSurrogate() && index + 1 < input.size() &&
        input.at(index + 1).isLowSurrogate()) {
      const QChar low = input.at(index + 1);
      const auto category =
          QChar::category(QChar::surrogateToUcs4(character, low));
      if (category == QChar::Other_Control ||
          category == QChar::Other_Format) {
        ++index;
        continue;
      }
      output.append(character);
      output.append(low);
      ++index;
      continue;
    }
    if (character.isLowSurrogate() || character.isHighSurrogate() ||
        character.isNull() || character.category() == QChar::Other_Control ||
        character.category() == QChar::Other_Format) {
      if (character.isSpace()) {
        output.append(QLatin1Char(' '));
      }
      continue;
    }
    output.append(character);
  }
  return output;
}

void trimTerminalPunctuation(QString *candidate) {
  while (!candidate->isEmpty()) {
    const QChar last = candidate->back();
    if (QStringLiteral(".,;:!?").contains(last)) {
      candidate->chop(1);
      continue;
    }
    if (last == QLatin1Char(')') &&
        candidate->count(QLatin1Char(')')) >
            candidate->count(QLatin1Char('('))) {
      candidate->chop(1);
      continue;
    }
    if (last == QLatin1Char(']') &&
        candidate->count(QLatin1Char(']')) >
            candidate->count(QLatin1Char('['))) {
      candidate->chop(1);
      continue;
    }
    break;
  }
}

[[nodiscard]] TerminalLink makeLink(TerminalLinkKind kind,
                                    const QString &target) {
  TerminalLink link;
  link.kind = kind;
  link.target = target;
  link.display = target;
  link.tooltip = kind == TerminalLinkKind::WebUrl
                     ? QStringLiteral(
                           "Exact target: %1\nThe hostname is shown exactly "
                           "as printed; Unicode and punycode are not normalized.")
                           .arg(target)
                     : QStringLiteral("Exact local path: %1").arg(target);
  return link;
}

} // namespace

bool isAdmittedTerminalLink(const TerminalLink &link) {
  if (link.target.isEmpty() || link.target.size() > kTerminalLinkTargetLimit ||
      stripControls(link.target) != link.target || link.display != link.target) {
    return false;
  }
  if (link.kind == TerminalLinkKind::LocalPath) {
    return QDir::isAbsolutePath(link.target);
  }
  const QUrl url(link.target, QUrl::StrictMode);
  return url.isValid() && !url.host().isEmpty() &&
         (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) ==
              0 ||
          url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) ==
              0);
}

QList<TerminalLink> detectTerminalLinks(const QString &visibleOutput) {
  const QString text = stripControls(visibleOutput.left(
      qMin(visibleOutput.size(), kTerminalVisibleOutputLimit)));
  QList<TerminalLink> links;
  qsizetype index = 0;
  while (index < text.size() && links.size() < kTerminalVisibleLinkLimit) {
    const bool boundary = index == 0 || isBoundary(text.at(index - 1));
    const bool web = boundary &&
                     (text.mid(index, 7).compare(QStringLiteral("http://"),
                                                 Qt::CaseInsensitive) == 0 ||
                      text.mid(index, 8).compare(QStringLiteral("https://"),
                                                 Qt::CaseInsensitive) == 0);
    const bool local = boundary && text.at(index) == QLatin1Char('/');
    if (!web && !local) {
      ++index;
      continue;
    }

    const QChar quote = index > 0 &&
                                (text.at(index - 1) == QLatin1Char('"') ||
                                 text.at(index - 1) == QLatin1Char('\''))
                            ? text.at(index - 1)
                            : QChar();
    qsizetype end = index;
    while (end < text.size()) {
      const QChar character = text.at(end);
      if ((!quote.isNull() && character == quote) ||
          (quote.isNull() &&
           (character.isSpace() || character == QLatin1Char('<') ||
            character == QLatin1Char('>') || character == QLatin1Char('{') ||
            character == QLatin1Char('}') || character == QLatin1Char('"') ||
            character == QLatin1Char('\'')))) {
        break;
      }
      ++end;
    }
    QString candidate = text.mid(index, end - index);
    trimTerminalPunctuation(&candidate);
    if (candidate.size() <= kTerminalLinkTargetLimit) {
      TerminalLink link = makeLink(web ? TerminalLinkKind::WebUrl
                                       : TerminalLinkKind::LocalPath,
                                   candidate);
      if (isAdmittedTerminalLink(link)) {
        links.append(std::move(link));
      }
    }
    index = qMax(end, index + 1);
  }
  return links;
}

} // namespace QindaQt::Apps::Terminal
