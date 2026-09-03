// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_widget_adapter.h"

#include "links/terminal_link.h"
#include "search/terminal_search.h"

#include <qtermwidget.h>

#include <QAction>
#include <QIODevice>
#include <QLineEdit>
#include <QMenu>
#include <QMetaObject>
#include <QSignalBlocker>
#include <QToolButton>

namespace QindaQt::Apps::Terminal {
namespace {

class BoundedHistoryDevice final : public QIODevice {
public:
  BoundedHistoryDevice(qsizetype limit, bool retainTail)
      : m_limit(limit), m_retainTail(retainTail) {
    open(QIODevice::WriteOnly);
  }

  [[nodiscard]] QString text() const { return QString::fromUtf8(m_bytes); }
  [[nodiscard]] bool overflowed() const { return m_overflowed; }

protected:
  qint64 readData(char *, qint64) override { return -1; }
  qint64 writeData(const char *data, qint64 length) override {
    if (length <= 0) {
      return length;
    }
    const qsizetype count = qsizetype{length};
    if (!m_retainTail) {
      const qsizetype available = qMax(qsizetype{0}, m_limit - m_bytes.size());
      m_bytes.append(data, qMin(count, available));
      m_overflowed = m_overflowed || count > available;
      return length;
    }
    if (count >= m_limit) {
      m_bytes = QByteArray(data + count - m_limit, m_limit);
      m_overflowed = true;
      return length;
    }
    m_bytes.append(data, count);
    if (m_bytes.size() > m_limit) {
      m_bytes.remove(0, m_bytes.size() - m_limit);
      m_overflowed = true;
    }
    return length;
  }

private:
  qsizetype m_limit;
  bool m_retainTail = false;
  bool m_overflowed = false;
  QByteArray m_bytes;
};

} // namespace

void TerminalWidgetAdapter::initializeSearchSurface() {
  // AGENT-CONTRACT (ADR-0040 S2 amendment): only this confined adapter knows
  // the pinned 2.4 SearchBar object tree. The application UI supplies its own
  // accessible bar and crosses this boundary with typed values only.
  if (m_widget == nullptr) {
    return;
  }
  m_searchEditor = m_widget->findChild<QLineEdit *>(
      QStringLiteral("searchTextEdit"));
  QToolButton *options =
      m_widget->findChild<QToolButton *>(QStringLiteral("optionsButton"));
  if (options == nullptr || options->menu() == nullptr ||
      options->menu()->actions().size() != 3) {
    m_searchEditor = nullptr;
    return;
  }
  const QList<QAction *> actions = options->menu()->actions();
  m_searchMatchCase = actions.at(0);
  m_searchRegex = actions.at(1);
  m_searchHighlightAll = actions.at(2);
}

QString TerminalWidgetAdapter::captureHistory(qsizetype byteLimit,
                                              bool retainTail,
                                              bool *overflow) const {
  BoundedHistoryDevice device(byteLimit, retainTail);
  if (m_widget != nullptr) {
    m_widget->saveHistory(&device);
  }
  if (overflow != nullptr) {
    *overflow = device.overflowed();
  }
  return device.text();
}

TerminalSearchResult TerminalWidgetAdapter::searchScrollback(
    const TerminalSearchQuery &query, TerminalSearchDirection direction) {
  if (m_widget == nullptr || m_searchEditor == nullptr ||
      m_searchMatchCase == nullptr || m_searchRegex == nullptr ||
      m_searchHighlightAll == nullptr) {
    return {.diagnostic = QStringLiteral("Renderer search is unavailable")};
  }
  bool overflow = false;
  const QString history = captureHistory(kTerminalSearchSnapshotByteLimit,
                                         false, &overflow);
  if (overflow) {
    return {.diagnostic =
                QStringLiteral("Scrollback exceeds the 4 MiB search limit")};
  }
  TerminalSearchScan scan = scanTerminalText(history, query);
  if (!scan.result.accepted) {
    return scan.result;
  }

  const bool changed = query != m_searchQuery;
  if (changed || direction == TerminalSearchDirection::Initial) {
    const QSignalBlocker editorBlocker(m_searchEditor);
    const QSignalBlocker caseBlocker(m_searchMatchCase);
    const QSignalBlocker regexBlocker(m_searchRegex);
    const QSignalBlocker highlightBlocker(m_searchHighlightAll);
    m_searchMatchCase->setChecked(query.caseSensitive);
    m_searchRegex->setChecked(query.regularExpression);
    m_searchHighlightAll->setChecked(true);
    m_searchEditor->setText(query.pattern);
    m_widget->setSelectionStart(0, 0);
    m_widget->setSelectionEnd(0, 0);
    const bool startBackward =
        direction == TerminalSearchDirection::Previous && scan.result.found;
    static_cast<void>(QMetaObject::invokeMethod(
        m_widget, startBackward ? "findPrevious" : "find",
        Qt::DirectConnection));
    m_searchQuery = query;
    m_searchMatchIndex = scan.result.found
                             ? (startBackward
                                    ? static_cast<int>(scan.matches.size()) - 1
                                    : 0)
                             : -1;
    m_searchRendererActive = scan.result.found;
    scan.result.wrapped = startBackward;
  } else if (scan.result.found) {
    if (!m_searchRendererActive) {
      // AGENT-GUARD: Escape removes renderer highlights, but the application
      // remembers this session's logical match. Rehydrate qtermwidget to that
      // exact match before applying Next/Previous; treating the query as new
      // makes Shift+F3 resume forward from match one (review P2-1).
      const QSignalBlocker editorBlocker(m_searchEditor);
      const QSignalBlocker caseBlocker(m_searchMatchCase);
      const QSignalBlocker regexBlocker(m_searchRegex);
      const QSignalBlocker highlightBlocker(m_searchHighlightAll);
      m_searchMatchCase->setChecked(query.caseSensitive);
      m_searchRegex->setChecked(query.regularExpression);
      m_searchHighlightAll->setChecked(true);
      m_searchEditor->setText(query.pattern);
      m_widget->setSelectionStart(0, 0);
      m_widget->setSelectionEnd(0, 0);
      static_cast<void>(QMetaObject::invokeMethod(
          m_widget, "find", Qt::DirectConnection));
      for (int index = 0; index < m_searchMatchIndex; ++index) {
        static_cast<void>(QMetaObject::invokeMethod(
            m_widget, "findNext", Qt::DirectConnection));
      }
      m_searchRendererActive = true;
    }
    if (m_searchMatchIndex < 0) {
      static_cast<void>(QMetaObject::invokeMethod(
          m_widget, "find", Qt::DirectConnection));
      m_searchMatchIndex = 0;
      scan.result.current = 1;
      return scan.result;
    }
    const bool previous = direction == TerminalSearchDirection::Previous;
    const int oldIndex = m_searchMatchIndex;
    const int matchCount = static_cast<int>(scan.matches.size());
    m_searchMatchIndex = previous
                             ? (m_searchMatchIndex - 1 + matchCount) % matchCount
                             : (m_searchMatchIndex + 1) % matchCount;
    scan.result.wrapped = previous ? m_searchMatchIndex > oldIndex
                                   : m_searchMatchIndex < oldIndex;
    static_cast<void>(QMetaObject::invokeMethod(
        m_widget, previous ? "findPrevious" : "findNext",
        Qt::DirectConnection));
  }
  scan.result.current = scan.result.found ? m_searchMatchIndex + 1 : 0;
  return scan.result;
}

void TerminalWidgetAdapter::clearScrollbackSearch() {
  if (m_widget == nullptr || m_searchEditor == nullptr) {
    return;
  }
  {
    const QSignalBlocker blocker(m_searchEditor);
    m_searchEditor->clear();
  }
  if (m_searchHighlightAll != nullptr) {
    const QSignalBlocker blocker(m_searchHighlightAll);
    m_searchHighlightAll->setChecked(false);
  }
  static_cast<void>(
      QMetaObject::invokeMethod(m_widget, "find", Qt::DirectConnection));
  static_cast<void>(QMetaObject::invokeMethod(
      m_widget, "noMatchFound", Qt::DirectConnection));
  // Query and logical index belong to the session even while highlights are
  // hidden. The next navigation request rehydrates renderer state from them.
  m_searchRendererActive = false;
}

QList<TerminalLink> TerminalWidgetAdapter::refreshVisibleLinks() {
  if (m_widget == nullptr) {
    m_visibleLinks.clear();
    m_visibleLinkIndex = -1;
    return {};
  }
  const bool hadSelection = m_visibleLinkIndex >= 0 &&
                            m_visibleLinkIndex < m_visibleLinks.size();
  const TerminalLink selected = hadSelection
                                    ? m_visibleLinks.at(m_visibleLinkIndex)
                                    : TerminalLink{};
  QString tail = captureHistory(kTerminalVisibleOutputLimit, true, nullptr);
  const QStringList lines = tail.split(QLatin1Char('\n'));
  const int visibleLineCount = qMax(1, m_widget->screenLinesCount());
  tail = lines.mid(qMax(0, lines.size() - visibleLineCount - 1)).join(
      QLatin1Char('\n'));
  m_visibleLinks = detectTerminalLinks(tail);
  if (m_visibleLinks.isEmpty()) {
    m_visibleLinkIndex = -1;
  } else if (!hadSelection) {
    m_visibleLinkIndex = 0;
  } else {
    m_visibleLinkIndex = static_cast<int>(m_visibleLinks.indexOf(selected));
  }
  return m_visibleLinks;
}

TerminalLinkSelection TerminalWidgetAdapter::selectVisibleLink(int delta) {
  static_cast<void>(refreshVisibleLinks());
  if (m_visibleLinks.isEmpty()) {
    return {};
  }
  if (m_visibleLinkIndex < 0) {
    m_visibleLinkIndex =
        delta < 0 ? static_cast<int>(m_visibleLinks.size()) - 1 : 0;
    return {.found = true,
            .current = m_visibleLinkIndex + 1,
            .total = static_cast<int>(m_visibleLinks.size()),
            .link = m_visibleLinks.at(m_visibleLinkIndex)};
  }
  const int oldIndex = m_visibleLinkIndex;
  const int linkCount = static_cast<int>(m_visibleLinks.size());
  m_visibleLinkIndex =
      (m_visibleLinkIndex + delta + linkCount) % linkCount;
  return {.found = true,
          .current = m_visibleLinkIndex + 1,
          .total = linkCount,
          .wrapped = delta < 0 ? m_visibleLinkIndex > oldIndex
                               : m_visibleLinkIndex < oldIndex,
          .link = m_visibleLinks.at(m_visibleLinkIndex)};
}

TerminalLinkSelection TerminalWidgetAdapter::currentVisibleLink() {
  static_cast<void>(refreshVisibleLinks());
  if (m_visibleLinks.isEmpty() || m_visibleLinkIndex < 0) {
    return {};
  }
  return {.found = true,
          .current = m_visibleLinkIndex + 1,
          .total = static_cast<int>(m_visibleLinks.size()),
          .link = m_visibleLinks.at(m_visibleLinkIndex)};
}

} // namespace QindaQt::Apps::Terminal
