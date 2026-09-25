// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "preferences_store.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: this GUI-thread QObject owns the injected PreferencesStore
// and publishes the current preferences for one process (ADR-0198). It
// applies nothing itself: QML binds the presentation defaults onto
// NavigationController, the composition root hands the discovery knob to
// DiscoveryController, and the dialogs read the rest. That separation is why
// this class has no dependency on any other controller.
//
// AGENT-GUARD: a refused write never changes the published values, so what
// the Preferences window shows is always what the next launch will read. A
// value outside its documented set is refused, never snapped -- see
// Preferences::isValid().
class PreferencesController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString defaultViewMode READ defaultViewMode NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(bool showHidden READ showHidden NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(bool directoriesFirst READ directoriesFirst NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(QString sortDirection READ sortDirection NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(int iconSize READ iconSize NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(bool discoverNearbyServers READ discoverNearbyServers NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(QString defaultConnectScheme READ defaultConnectScheme NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(bool confirmTrash READ confirmTrash NOTIFY preferencesChanged FINAL)
  // ADR-0270 (preferences-v2): the rest of the default folder view and the
  // Details view's window-wide presentation.
  Q_PROPERTY(QString groupBy READ groupBy NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(QVariantList detailsColumns READ detailsColumns NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(bool relativeDates READ relativeDates NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(QString rowDensity READ rowDensity NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(bool showExtensions READ showExtensions NOTIFY preferencesChanged FINAL)
  Q_PROPERTY(QString storeError READ storeError NOTIFY storeErrorChanged FINAL)
  // The accepted values, so the window's pickers cannot drift from the schema.
  Q_PROPERTY(QStringList viewModes READ viewModes CONSTANT FINAL)
  Q_PROPERTY(QStringList sortColumns READ sortColumns CONSTANT FINAL)
  Q_PROPERTY(QStringList sortDirections READ sortDirections CONSTANT FINAL)
  Q_PROPERTY(QVariantList iconSizes READ iconSizes CONSTANT FINAL)
  Q_PROPERTY(QStringList schemes READ schemes CONSTANT FINAL)
  Q_PROPERTY(QStringList groupKeys READ groupKeys CONSTANT FINAL)
  Q_PROPERTY(QStringList columnKeys READ columnKeys CONSTANT FINAL)
  Q_PROPERTY(QStringList rowDensities READ rowDensities CONSTANT FINAL)

public:
  explicit PreferencesController(std::unique_ptr<PreferencesStore> store,
                                 QObject *parent = nullptr);

  Q_INVOKABLE void setDefaultViewMode(const QString &mode);
  Q_INVOKABLE void setShowHidden(bool showHidden);
  Q_INVOKABLE void setDirectoriesFirst(bool directoriesFirst);
  Q_INVOKABLE void setSortColumn(const QString &column);
  Q_INVOKABLE void setSortDirection(const QString &direction);
  Q_INVOKABLE void setIconSize(int iconSize);
  Q_INVOKABLE void setDiscoverNearbyServers(bool discover);
  Q_INVOKABLE void setDefaultConnectScheme(const QString &scheme);
  Q_INVOKABLE void setConfirmTrash(bool confirmTrash);
  Q_INVOKABLE void setGroupBy(const QString &groupBy);
  // [{key, width}], Name first (see Preferences::columnKeys()).
  Q_INVOKABLE void setDetailsColumns(const QVariantList &columns);
  Q_INVOKABLE void setRelativeDates(bool relativeDates);
  Q_INVOKABLE void setRowDensity(const QString &rowDensity);
  Q_INVOKABLE void setShowExtensions(bool showExtensions);

  // ADR-0270: per-folder views. A view crosses to QML as {viewMode,
  // sortColumn, sortDirection, groupBy, iconSize, columns: [{key, width}]}.
  // folderView() is `location`'s own view when remembered, else the
  // defaults, plus `remembered`; defaultFolderView() is the defaults alone.
  Q_INVOKABLE [[nodiscard]] QVariantMap folderView(const QString &location) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap defaultFolderView() const;
  // Remembers the view the user gave `location` (a view equal to the
  // defaults forgets it instead). Returns false when refused; nothing is
  // written when `location` already shows exactly this view.
  Q_INVOKABLE bool rememberFolderView(const QString &location, const QVariantMap &view);
  // Makes `view` the defaults ("Use as Defaults") and forgets every folder
  // whose own view now equals them.
  Q_INVOKABLE bool useAsDefaults(const QVariantMap &view);
  // Returns every value to the documented default and persists that.
  Q_INVOKABLE void restoreDefaults();
  Q_INVOKABLE void clearStoreError();

  [[nodiscard]] QString defaultViewMode() const { return m_preferences.defaultViewMode; }
  [[nodiscard]] bool showHidden() const { return m_preferences.showHidden; }
  [[nodiscard]] bool directoriesFirst() const { return m_preferences.directoriesFirst; }
  [[nodiscard]] QString sortColumn() const { return m_preferences.sortColumn; }
  [[nodiscard]] QString sortDirection() const { return m_preferences.sortDirection; }
  [[nodiscard]] int iconSize() const { return m_preferences.iconSize; }
  [[nodiscard]] bool discoverNearbyServers() const {
    return m_preferences.discoverNearbyServers;
  }
  [[nodiscard]] QString defaultConnectScheme() const {
    return m_preferences.defaultConnectScheme;
  }
  [[nodiscard]] bool confirmTrash() const { return m_preferences.confirmTrash; }
  [[nodiscard]] QString groupBy() const { return m_preferences.groupBy; }
  [[nodiscard]] QVariantList detailsColumns() const;
  [[nodiscard]] bool relativeDates() const { return m_preferences.relativeDates; }
  [[nodiscard]] QString rowDensity() const { return m_preferences.rowDensity; }
  [[nodiscard]] bool showExtensions() const { return m_preferences.showExtensions; }
  [[nodiscard]] QStringList groupKeys() const { return Preferences::groupKeys(); }
  [[nodiscard]] QStringList columnKeys() const { return Preferences::columnKeys(); }
  [[nodiscard]] QStringList rowDensities() const { return Preferences::rowDensities(); }
  [[nodiscard]] QString storeError() const { return m_storeError; }
  [[nodiscard]] QStringList viewModes() const { return Preferences::viewModes(); }
  [[nodiscard]] QStringList sortColumns() const { return Preferences::sortColumns(); }
  [[nodiscard]] QStringList sortDirections() const {
    return Preferences::sortDirections();
  }
  [[nodiscard]] QVariantList iconSizes() const;
  [[nodiscard]] QStringList schemes() const;

  // Test seam independent of QML's marshalling.
  [[nodiscard]] Preferences values() const { return m_preferences; }

signals:
  void preferencesChanged();
  void storeErrorChanged();

private:
  bool commit(const Preferences &candidate);
  void setStoreError(const QString &message);

  std::unique_ptr<PreferencesStore> m_store;
  Preferences m_preferences;
  QString m_storeError;
};

} // namespace QindaQt::Apps::FileManager
