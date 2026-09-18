// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "network_locations_store.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <memory>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: this GUI-thread QObject owns the injected
// NetworkLocationsStore and the saved-location list for one window. It
// publishes complete snapshots plus one typed store-error string; it never
// navigates, lists a folder, contacts a server, or shows UI. QML activates a
// location by handing its published `url` to NavigationController, so a
// location whose server is unreachable lands on the ordinary navigation
// state card rather than inventing a second error channel -- the same
// division PlacesController already uses for bookmarks.
//
// AGENT-GUARD: a refused write never changes the published list. The
// in-memory list is only replaced after the store accepts it, so the sidebar
// can never show a location that is not on disk.
class NetworkLocationsController final : public QObject {
  Q_OBJECT

  // Every saved location, newest last: {id, name, url, showInPlaces, index}.
  Q_PROPERTY(QVariantList locations READ locations NOTIFY locationsChanged FINAL)
  // The subset whose showInPlaces is true, for the Places sidebar.
  Q_PROPERTY(QVariantList placesLocations READ placesLocations NOTIFY locationsChanged FINAL)
  Q_PROPERTY(QString storeError READ storeError NOTIFY storeErrorChanged FINAL)
  // The last Connect-to-server refusal, empty while the dialog has not been
  // refused. Cleared by the next accepted save or by clearRequestError().
  Q_PROPERTY(QString requestError READ requestError NOTIFY requestErrorChanged FINAL)
  Q_PROPERTY(QStringList schemes READ schemes CONSTANT FINAL)

public:
  explicit NetworkLocationsController(std::unique_ptr<NetworkLocationsStore> store,
                                      QObject *parent = nullptr);

  // Builds one location from Connect-to-server dialog text (scheme, host,
  // port, remotePath, displayName, showInPlaces), saves it, and returns the
  // canonical URL string on success or an empty string on refusal, with
  // requestError() set. Re-saving the same canonical address replaces the
  // existing record's name and Places flag instead of duplicating it.
  Q_INVOKABLE QString saveLocation(const QVariantMap &request);
  Q_INVOKABLE void removeLocation(int index);
  Q_INVOKABLE void clearStoreError();
  Q_INVOKABLE void clearRequestError();

  [[nodiscard]] QVariantList locations() const;
  [[nodiscard]] QVariantList placesLocations() const;
  [[nodiscard]] QString storeError() const { return m_storeError; }
  [[nodiscard]] QString requestError() const { return m_requestError; }
  [[nodiscard]] QStringList schemes() const;

  // Test seam independent of QML's QVariantList marshalling.
  [[nodiscard]] QVector<NetworkLocationRecord> locationValues() const {
    return m_locations;
  }

signals:
  void locationsChanged();
  void storeErrorChanged();
  void requestErrorChanged();

private:
  void setStoreError(const QString &message);
  void setRequestError(const QString &message);
  // Persists `candidate`; on refusal publishes the store error and leaves
  // the visible list untouched.
  bool persist(const QVector<NetworkLocationRecord> &candidate);

  std::unique_ptr<NetworkLocationsStore> m_store;
  QVector<NetworkLocationRecord> m_locations;
  QString m_storeError;
  QString m_requestError;
};

} // namespace QindaQt::Apps::FileManager
