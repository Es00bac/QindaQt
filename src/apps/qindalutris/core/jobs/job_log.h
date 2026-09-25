// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the "Copy details" text of ADR-0275 section 5. Every job
// stage appends here; the UI shows ONE plain sentence and offers this text
// only behind "Copy details" for someone helping. It is bounded (oldest lines
// are dropped, the header stays) and control characters are flattened, so a
// hostile installer's output cannot flood memory or forge log lines.
// AGENT-GUARD: never append secrets, cookies or full environments -- the
// text is meant to be pasted into a chat with a stranger.
class JobLog final {
public:
  static constexpr qsizetype kMaxLines = 4000;
  static constexpr qsizetype kMaxLineChars = 2000;

  explicit JobLog(const QString &header = {});

  void reset(const QString &header);
  // Appends one UTC-timestamped line (multi-line input becomes several).
  void append(const QString &text);

  [[nodiscard]] QString text() const;
  [[nodiscard]] const QStringList &lines() const { return m_lines; }

private:
  QString m_header;
  QStringList m_lines;
  qsizetype m_dropped = 0;
};

} // namespace QindaQt::QindaLutris
