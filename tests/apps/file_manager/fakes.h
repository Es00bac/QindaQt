// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "model/directory_lister.h"
#include "model/launch_intent.h"

#include <QHash>
#include <QStringList>

namespace QindaQt::Apps::FileManager::Test {

// Test-only DirectoryLister that answers from a caller-populated table instead
// of touching the real filesystem, so NavigationController's dispatch and
// status-mapping logic can be verified independently of LocalDirectoryLister.
class FakeDirectoryLister final : public DirectoryLister {
public:
  [[nodiscard]] ListingResult list(const QString &absolutePath) const override {
    m_requestedPaths.append(absolutePath);
    const auto it = m_results.constFind(absolutePath);
    if (it == m_results.constEnd()) {
      ListingResult notFound;
      notFound.path = absolutePath;
      notFound.error = ListingError::NotFound;
      notFound.diagnostic = QStringLiteral("%1 was not configured in the fake lister")
                                .arg(absolutePath);
      return notFound;
    }
    return it.value();
  }

  void setResult(const QString &absolutePath, ListingResult result) {
    m_results.insert(absolutePath, std::move(result));
  }

  [[nodiscard]] const QStringList &requestedPaths() const { return m_requestedPaths; }

private:
  mutable QHash<QString, ListingResult> m_results;
  mutable QStringList m_requestedPaths;
};

// Test-only FileLauncher that never touches a real desktop handler; it just
// records the requested path and returns a caller-configured canned result.
class FakeFileLauncher final : public FileLauncher {
public:
  [[nodiscard]] LaunchResult launch(const QString &absolutePath) const override {
    m_requestedPaths.append(absolutePath);
    return m_result;
  }

  void setResult(LaunchResult result) { m_result = std::move(result); }

  [[nodiscard]] const QStringList &requestedPaths() const { return m_requestedPaths; }

private:
  LaunchResult m_result;
  mutable QStringList m_requestedPaths;
};

} // namespace QindaQt::Apps::FileManager::Test
