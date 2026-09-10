// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <atomic>
#include <memory>

class QThread;

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: GUI-thread confined owner of the properties-dialog state.
// Single-entry fields are filled synchronously from the listing snapshot plus
// a MIME lookup; folder/multi total sizes are walked on one bounded worker
// thread that is cancelled and joined on re-inspect, clear(), or destruction.
// This object never mutates the filesystem.
class EntryPropertiesController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool active READ active NOTIFY stateChanged FINAL)
  Q_PROPERTY(int entryCount READ entryCount NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString name READ name NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString kindText READ kindText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString mimeText READ mimeText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString sizeText READ sizeText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString modifiedText READ modifiedText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString permissionsText READ permissionsText NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString pathText READ pathText NOTIFY stateChanged FINAL)
  // Total-size walk across the selection's folders: running state, published
  // total, and whether a bound cut the walk short.
  Q_PROPERTY(bool computingTotal READ computingTotal NOTIFY stateChanged FINAL)
  Q_PROPERTY(QString totalSizeText READ totalSizeText NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool totalTruncated READ totalTruncated NOTIFY stateChanged FINAL)

public:
  explicit EntryPropertiesController(QObject *parent = nullptr);
  ~EntryPropertiesController() override;

  // Bounds for the total-size walk: visited entries and folder depth per
  // selection root. Symbolic links are never descended.
  static constexpr int maximumVisited = 20000;
  static constexpr int maximumDepth = 32;

  // Entries are the QML listing snapshots (name/path/isDirectory/isSymlink/
  // size/modified/mode keys). An empty list clears the dialog state.
  Q_INVOKABLE void inspect(const QVariantList &entries);
  Q_INVOKABLE void clear();

  [[nodiscard]] bool active() const { return m_active; }
  [[nodiscard]] int entryCount() const { return m_entryCount; }
  [[nodiscard]] QString name() const { return m_name; }
  [[nodiscard]] QString kindText() const { return m_kindText; }
  [[nodiscard]] QString mimeText() const { return m_mimeText; }
  [[nodiscard]] QString sizeText() const { return m_sizeText; }
  [[nodiscard]] QString modifiedText() const { return m_modifiedText; }
  [[nodiscard]] QString permissionsText() const { return m_permissionsText; }
  [[nodiscard]] QString pathText() const { return m_pathText; }
  [[nodiscard]] bool computingTotal() const { return m_computingTotal; }
  [[nodiscard]] QString totalSizeText() const { return m_totalSizeText; }
  [[nodiscard]] bool totalTruncated() const { return m_totalTruncated; }

signals:
  void stateChanged();

private:
  void stopWorker();
  void startTotalWalk(const QVariantList &entries);

  bool m_active = false;
  int m_entryCount = 0;
  QString m_name;
  QString m_kindText;
  QString m_mimeText;
  QString m_sizeText;
  QString m_modifiedText;
  QString m_permissionsText;
  QString m_pathText;
  bool m_computingTotal = false;
  QString m_totalSizeText;
  bool m_totalTruncated = false;
  QThread *m_worker = nullptr;
  std::shared_ptr<std::atomic_bool> m_cancellation;
  // Fences queued totalReady deliveries against workers stopWorker() already
  // disposed; bumped on every stop/start.
  quint64 m_generation = 0;
};

} // namespace QindaQt::Apps::FileManager
