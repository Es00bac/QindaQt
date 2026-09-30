// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>
#include <optional>
namespace QindaQt::WorkspacesApps {
struct DesktopApplication {
  QString id;
  QString name;
  QString iconName;
};
// GUI-thread adapter owned by its caller. Uses the XDG application catalog;
// never interprets saved application intent as a shell command. Launch is
// asynchronous: true means dispatched, launchFinished reports process startup,
// and neither means the compositor has received a usable window yet.
class DesktopApplications final : public QObject {
  Q_OBJECT
public:
  explicit DesktopApplications(QObject *parent = nullptr);
  [[nodiscard]] std::optional<DesktopApplication>
  find(const QString &desktopEntryId) const;
  // Uses one cached public XDG catalog scan. Exact desktop-entry/app ids win;
  // otherwise a unique case-insensitive StartupWMClass alias may resolve.
  // Ambiguous aliases return empty. GUI-thread only.
  [[nodiscard]] std::optional<DesktopApplication> findForWindow(
      const QString &desktopEntryId, const QString &applicationId,
      const QString &resourceClass) const;
  [[nodiscard]] bool launch(const QString &desktopEntryId,
                            const QStringList &urls,
                            const QByteArray &activationToken = {},
                            QString *error = nullptr);
Q_SIGNALS:
  void launchFinished(const QString &desktopEntryId, bool started,
                      const QString &error);

private:
  QVector<DesktopApplication> m_catalogApplications;
  QHash<QString, qsizetype> m_catalogByExactId;
  QHash<QString, QStringList> m_idsByStartupWmClass;
};
} // namespace QindaQt::WorkspacesApps
