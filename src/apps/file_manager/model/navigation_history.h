// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::Apps::FileManager {

struct BreadcrumbSegment final {
  QString name;
  QString path;

  [[nodiscard]] bool operator==(const BreadcrumbSegment &) const = default;
};

// This value owns only back/forward/current-path bookkeeping. It performs no
// I/O and displays no UI, so tests can verify navigation truth without a
// filesystem, mirroring DocumentState's separation of policy from I/O.
class NavigationHistory final {
public:
  [[nodiscard]] bool hasCurrent() const;
  [[nodiscard]] const QString &currentPath() const;
  [[nodiscard]] bool canGoBack() const;
  [[nodiscard]] bool canGoForward() const;

  // Replaces the whole history, as if freshly launched at path. Used only for
  // the first navigation of a window's lifetime.
  void reset(QString path);

  // Pushes the current path onto the back stack, clears the forward stack,
  // and adopts path as current. Returns false without mutating anything when
  // path already is the current path.
  bool navigateTo(QString path);

  [[nodiscard]] std::optional<QString> goBack();
  [[nodiscard]] std::optional<QString> goForward();

  // Pure lexical parent computation (QDir::cleanPath/rootPath never touch the
  // filesystem); returns nullopt at the filesystem root.
  [[nodiscard]] static std::optional<QString>
  parentOf(const QString &absolutePath);

  // Splits a cleaned absolute path into root-first cumulative segments for a
  // breadcrumb bar, e.g. "/home/jarrod" -> [{"/","/"}, {"home","/home"},
  // {"jarrod","/home/jarrod"}].
  [[nodiscard]] static QVector<BreadcrumbSegment>
  breadcrumbFor(const QString &absolutePath);

private:
  std::optional<QString> m_current;
  QVector<QString> m_back;
  QVector<QString> m_forward;
};

} // namespace QindaQt::Apps::FileManager
