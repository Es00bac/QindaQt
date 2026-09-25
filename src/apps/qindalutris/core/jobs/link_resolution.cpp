// SPDX-License-Identifier: GPL-3.0-or-later
#include "link_resolution.h"

namespace QindaQt::QindaLutris {

namespace {

// Walks `target` from `current`; every component that is a link is replaced
// by its own confined resolution. `hops` counts links followed overall.
std::optional<QStringList> walk(QStringList current, const QString &target,
                                const LinkReader &readLink, int *hops) {
  if (target.isEmpty() || target.startsWith(QLatin1Char('/')) || current.isEmpty()) {
    return std::nullopt;
  }
  const QString top = current.first();
  for (const QString &part : target.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
    if (part == QLatin1String(".")) {
      continue;
    }
    if (part == QLatin1String("..")) {
      if (current.size() <= 1) {
        return std::nullopt; // above the top folder
      }
      current.removeLast();
      continue;
    }
    current.append(part);
    if (const auto link = readLink(current)) {
      if (++*hops > kMaxLinkHops) {
        return std::nullopt;
      }
      current.removeLast();
      const auto resolved = walk(current, *link, readLink, hops);
      if (!resolved || resolved->isEmpty() || resolved->first() != top) {
        return std::nullopt;
      }
      current = *resolved;
    }
  }
  return current;
}

} // namespace

std::optional<QStringList> resolveConfined(const QStringList &parent, const QString &target,
                                           const LinkReader &readLink) {
  int hops = 0;
  return walk(parent, target, readLink, &hops);
}

std::optional<QStringList> resolvePathConfined(const QStringList &path,
                                               const LinkReader &readLink) {
  if (path.isEmpty()) {
    return std::nullopt;
  }
  int hops = 0;
  // Walk every component after the top folder through the same rules.
  return walk({path.first()}, path.mid(1).join(QLatin1Char('/')).isEmpty()
                                  ? QStringLiteral(".")
                                  : path.mid(1).join(QLatin1Char('/')),
              readLink, &hops);
}

} // namespace QindaQt::QindaLutris
