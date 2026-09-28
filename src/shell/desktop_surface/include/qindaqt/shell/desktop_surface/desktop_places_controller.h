// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <qqmlintegration.h>

#include <memory>
#include <optional>

namespace QindaQt::Apps::FileManager {
class MutationController;
}

namespace QindaQt::Shell::DesktopSurface {

// Where each standard desktop icon points. `standard()` resolves the XDG user
// directories through QStandardPaths and the home Trash exactly where File
// Manager keeps it ($XDG_DATA_HOME/Trash/files); tests inject a temporary tree.
struct DesktopPlaceLocations final {
  QString home;
  QString documents;
  QString downloads;
  QString pictures;
  QString videos;
  QString music;
  QString trashFiles;
  // "Computer" opens the root file system, which File Manager calls File
  // System. There is no Network icon: File Manager has no folder to open for
  // it (its Network place is the location bar).
  QString computer;

  [[nodiscard]] static DesktopPlaceLocations standard();
};

// The optional standard desktop icons (Home, Documents, Downloads, Pictures,
// Videos, Music, Trash, Computer), published as rows the desktop icon view
// shows before the Desktop folder's own entries.
//
// AGENT-CONTRACT: each icon is one boolean desktop-icons applet setting
// (settingKey()), declared with its default in data/applets/desktop-icons.json
// -- Home and Trash on, the rest off -- so the existing applet-settings path
// (the desktop's Customize menu, the Customize editor, profiles, Settings1)
// stores and edits them; this controller owns no storage. A folder that does
// not exist, or that is the home folder itself (an unset XDG directory), shows
// no icon. The Trash icon always shows when enabled, reading user-trash or
// user-trash-full from its files folder. Rows are {id, placeId, label, path,
// iconName, accessibleName, isDirectory: true, isPlace: true, layoutKey}; ids
// and layout keys are "place:<placeId>", disjoint from Desktop-folder paths.
// A place is never renamed, cut, copied or trashed: hiding one is its setting.
//
// Opening goes through File Manager's FileBoundary like every other desktop
// folder. Files dropped on the Trash icon are moved to the home Trash by the
// same identity-checked mutation authority File Manager uses; nothing is ever
// deleted permanently here.
//
// Not final: QML_ELEMENT instantiates the type through a QQmlElement subclass.
class DesktopPlacesController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  // The desktop-icons applet settings map (defaults apply to missing keys).
  Q_PROPERTY(QVariantMap settings READ settings WRITE setSettings NOTIFY settingsChanged)
  Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
  Q_PROPERTY(bool trashFull READ trashFull NOTIFY rowsChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  explicit DesktopPlacesController(QObject *parent = nullptr);
  // Test seam: fixed locations and (when set) File Manager program candidates.
  DesktopPlacesController(DesktopPlaceLocations locations,
                          std::optional<QStringList> fileManagerPrograms,
                          QObject *parent = nullptr);
  ~DesktopPlacesController() override;

  // Every place in desktop order, and each one's settings key and default.
  [[nodiscard]] static QStringList placeIds();
  [[nodiscard]] static QString settingKey(const QString &placeId);
  [[nodiscard]] static bool shownByDefault(const QString &placeId);

  [[nodiscard]] QVariantMap settings() const { return m_settings; }
  void setSettings(const QVariantMap &settings);
  [[nodiscard]] QVariantList rows() const { return m_rows; }
  [[nodiscard]] bool trashFull() const { return m_trashFull; }
  [[nodiscard]] QString feedback() const { return m_feedback; }

  Q_INVOKABLE bool isPlace(const QString &rowId) const;
  // Opens a shown place in File Manager. The Trash folder is created on first
  // open when nothing has been trashed yet (it is empty, not missing).
  Q_INVOKABLE bool open(const QString &rowId);
  // Moves dropped local files to the home Trash. Every URL must be a local
  // file outside the Trash that its folder's listing still reports; anything
  // else refuses the whole drop with `feedback` before anything moves.
  Q_INVOKABLE bool trashUrls(const QVariantList &urls);
  Q_INVOKABLE void refresh();

Q_SIGNALS:
  void settingsChanged();
  void rowsChanged();
  void feedbackChanged();

private:
  [[nodiscard]] QString locationFor(const QString &placeId) const;
  [[nodiscard]] bool shown(const QString &placeId) const;
  void watchTrash();
  void publishFeedback(const QString &message);

  DesktopPlaceLocations m_locations;
  std::optional<QStringList> m_fileManagerPrograms;
  QVariantMap m_settings;
  QVariantList m_rows;
  bool m_trashFull = false;
  QString m_feedback;
  QFileSystemWatcher m_watcher;
  std::unique_ptr<QindaQt::Apps::FileManager::MutationController> m_mutation;
};

} // namespace QindaQt::Shell::DesktopSurface
