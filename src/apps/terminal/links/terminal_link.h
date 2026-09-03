// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

namespace QindaQt::Apps::Terminal {

enum class TerminalLinkKind { WebUrl, LocalPath };

struct TerminalLink final {
  TerminalLinkKind kind = TerminalLinkKind::WebUrl;
  QString target;
  QString display;
  QString tooltip;

  [[nodiscard]] bool operator==(const TerminalLink &) const = default;
};

struct TerminalLinkSelection final {
  bool found = false;
  int current = 0;
  int total = 0;
  bool wrapped = false;
  TerminalLink link;
};

// Detects only explicit http(s) URLs and absolute local paths in a bounded
// visible-output snapshot. Returned display and argv target are the same
// control-free text; IDN and punycode spelling is never rewritten.
[[nodiscard]] QList<TerminalLink>
detectTerminalLinks(const QString &visibleOutput);
[[nodiscard]] bool isAdmittedTerminalLink(const TerminalLink &link);

inline constexpr qsizetype kTerminalVisibleOutputLimit = 512 * 1024;
inline constexpr qsizetype kTerminalLinkTargetLimit = 2048;
inline constexpr int kTerminalVisibleLinkLimit = 256;

} // namespace QindaQt::Apps::Terminal
