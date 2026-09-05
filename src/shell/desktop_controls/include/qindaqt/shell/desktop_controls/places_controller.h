// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>

namespace QindaQt::Shell::DesktopControls {

// One entry of the Places menu. `path` is an absolute directory the shell
// observed to exist at inventory time; `iconName` is an XDG icon name.
struct PlaceEntry {
  QString id;
  QString label;
  QString path;
  QString iconName;

  friend bool operator==(const PlaceEntry &, const PlaceEntry &) = default;
};

// Injected folder-opening seam. Production spawns the QindaQt File Manager
// through the launcher's `LaunchSpawner`; tests inject a recording fake.
// Implementations never invoke a shell and never open a non-directory.
class FolderOpener {
public:
  struct Result {
    bool ok = false;
    QString diagnostic;

    friend bool operator==(const Result &, const Result &) = default;
  };

  virtual ~FolderOpener() = default;
  [[nodiscard]] virtual Result open(const QString &absoluteDirectory) = 0;
};

// Builds the standard XDG user-directory inventory (Home, Desktop, Documents,
// Downloads, Music, Pictures, Videos) plus "Computer" (the filesystem root).
// Entries whose directory does not exist are omitted; duplicate paths keep
// the first label. `directoryExists` is injectable for deterministic tests.
[[nodiscard]] QList<PlaceEntry> standardPlaces(
    const std::function<bool(const QString &)> &directoryExists = {});

// Facade for the Places menu. Holds the bounded inventory and dispatches an
// open request exactly once through the injected seam.
//
// AGENT-CONTRACT: the borrowed opener may be null (visibly unavailable) and
// must otherwise outlive this controller on the GUI thread.
class PlacesController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows READ rows CONSTANT)
  Q_PROPERTY(int count READ count CONSTANT)
  Q_PROPERTY(bool available READ available CONSTANT)
  Q_PROPERTY(bool launchGranted READ launchGranted CONSTANT)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  static constexpr int MaxPlaces = 32;

  PlacesController(FolderOpener *opener, bool applicationsLaunchGranted,
                   QList<PlaceEntry> entries, QObject *parent = nullptr);

  // Rows: {id, label, path, iconName, accessibleName, index}.
  [[nodiscard]] QVariantList rows() const;
  [[nodiscard]] int count() const noexcept
  {
    return static_cast<int>(m_entries.size());
  }
  [[nodiscard]] bool available() const noexcept
  {
    return m_opener != nullptr && m_launchGranted && !m_entries.isEmpty();
  }
  [[nodiscard]] bool launchGranted() const noexcept { return m_launchGranted; }
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }
  [[nodiscard]] const QList<PlaceEntry> &entries() const noexcept { return m_entries; }

  Q_INVOKABLE bool open(const QString &placeId);
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void feedbackChanged();

private:
  void publishFeedback(const QString &message);

  FolderOpener *m_opener = nullptr;
  bool m_launchGranted = false;
  QList<PlaceEntry> m_entries;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopControls
