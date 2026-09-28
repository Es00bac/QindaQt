// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0282: the optional standard desktop icons over a temporary home tree,
// a redirected XDG_DATA_HOME Trash, and a recording File Manager stand-in.
#include "qindaqt/shell/desktop_surface/desktop_places_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <memory>

using QindaQt::Shell::DesktopSurface::DesktopPlaceLocations;
using QindaQt::Shell::DesktopSurface::DesktopPlacesController;

namespace {

[[nodiscard]] bool writeFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly);
}

[[nodiscard]] QStringList placeIdsOf(const QVariantList &rows) {
  QStringList ids;
  for (const QVariant &row : rows) {
    ids.append(row.toMap().value(QStringLiteral("placeId")).toString());
  }
  return ids;
}

[[nodiscard]] QVariantMap rowFor(const QVariantList &rows, const QString &placeId) {
  for (const QVariant &row : rows) {
    if (row.toMap().value(QStringLiteral("placeId")).toString() == placeId) {
      return row.toMap();
    }
  }
  return {};
}

[[nodiscard]] QString writeRecordingProgram(const QString &directory, const QString &record) {
  const QString program = directory + QStringLiteral("/qindaqt-file-manager");
  QFile file(program);
  if (!file.open(QIODevice::WriteOnly)) {
    return {};
  }
  file.write(QStringLiteral("#!/bin/sh\nprintf '%s\\0' \"$@\" > '%1.tmp' && mv '%1.tmp' '%1'\n")
                 .arg(record)
                 .toLocal8Bit());
  file.close();
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
  return program;
}

[[nodiscard]] QByteArray recorded(const QString &record) {
  QFile file(record);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class DesktopPlacesControllerTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void init();
  void cleanup();

  void homeAndTrashShowByDefault();
  void eachIconIsItsOwnSetting();
  void missingOrUnsetFoldersShowNoIcon();
  void rowsCarryStandardIconNamesAndAccessibleNames();
  void trashIconFollowsItsContents();
  void openingAPlaceStartsFileManagerOnItsFolder();
  void openingAnEmptyTrashCreatesItFirst();
  void openRefusesAnythingThatIsNotAShownPlace();
  void droppedFilesMoveToTheTrashNeverDeleted();
  void dropsFromInsideTheTrashOrNotLocalAreRefused();
  void manifestDeclaresEverySettingWithItsDefault();

private:
  [[nodiscard]] DesktopPlaceLocations locations() const;

  std::unique_ptr<QTemporaryDir> m_home;
  std::unique_ptr<QTemporaryDir> m_data;
  std::unique_ptr<QTemporaryDir> m_bin;
  QByteArray m_previousDataHome;
  QString m_record;
  QString m_program;
};

void DesktopPlacesControllerTests::init() {
  m_home = std::make_unique<QTemporaryDir>();
  m_data = std::make_unique<QTemporaryDir>();
  m_bin = std::make_unique<QTemporaryDir>();
  QVERIFY(m_home->isValid() && m_data->isValid() && m_bin->isValid());
  for (const char *folder : {"Documents", "Downloads", "Pictures", "Videos", "Music"}) {
    QVERIFY(QDir().mkpath(m_home->filePath(QString::fromLatin1(folder))));
  }
  // The mutation authority resolves the home Trash from XDG_DATA_HOME.
  m_previousDataHome = qgetenv("XDG_DATA_HOME");
  qputenv("XDG_DATA_HOME", m_data->path().toLocal8Bit());
  m_record = m_bin->filePath(QStringLiteral("argv"));
  m_program = writeRecordingProgram(m_bin->path(), m_record);
  QVERIFY(!m_program.isEmpty());
}

void DesktopPlacesControllerTests::cleanup() {
  if (m_previousDataHome.isNull()) {
    qunsetenv("XDG_DATA_HOME");
  } else {
    qputenv("XDG_DATA_HOME", m_previousDataHome);
  }
  m_home.reset();
  m_data.reset();
  m_bin.reset();
}

DesktopPlaceLocations DesktopPlacesControllerTests::locations() const {
  return {.home = m_home->path(),
          .documents = m_home->filePath(QStringLiteral("Documents")),
          .downloads = m_home->filePath(QStringLiteral("Downloads")),
          .pictures = m_home->filePath(QStringLiteral("Pictures")),
          .videos = m_home->filePath(QStringLiteral("Videos")),
          .music = m_home->filePath(QStringLiteral("Music")),
          .trashFiles = m_data->filePath(QStringLiteral("Trash/files")),
          .computer = QStringLiteral("/")};
}

void DesktopPlacesControllerTests::homeAndTrashShowByDefault() {
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QCOMPARE(placeIdsOf(controller.rows()),
           (QStringList{QStringLiteral("home"), QStringLiteral("trash")}));
  QVERIFY(DesktopPlacesController::shownByDefault(QStringLiteral("home")));
  QVERIFY(DesktopPlacesController::shownByDefault(QStringLiteral("trash")));
  QVERIFY(!DesktopPlacesController::shownByDefault(QStringLiteral("documents")));
}

void DesktopPlacesControllerTests::eachIconIsItsOwnSetting() {
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QSignalSpy rowsChanged(&controller, &DesktopPlacesController::rowsChanged);
  QVariantMap settings;
  for (const QString &placeId : DesktopPlacesController::placeIds()) {
    settings.insert(DesktopPlacesController::settingKey(placeId), true);
  }
  controller.setSettings(settings);
  QCOMPARE(rowsChanged.count(), 1);
  QCOMPARE(placeIdsOf(controller.rows()), DesktopPlacesController::placeIds());

  settings.insert(QStringLiteral("showHomeIcon"), false);
  settings.insert(QStringLiteral("showTrashIcon"), false);
  controller.setSettings(settings);
  QVERIFY(!placeIdsOf(controller.rows()).contains(QStringLiteral("home")));
  QVERIFY(!placeIdsOf(controller.rows()).contains(QStringLiteral("trash")));
  QVERIFY(placeIdsOf(controller.rows()).contains(QStringLiteral("music")));

  // A malformed value takes the default instead of hiding the icon.
  controller.setSettings({{QStringLiteral("showHomeIcon"), QStringLiteral("yes")}});
  QVERIFY(placeIdsOf(controller.rows()).contains(QStringLiteral("home")));
}

void DesktopPlacesControllerTests::missingOrUnsetFoldersShowNoIcon() {
  auto where = locations();
  QVERIFY(QDir(where.videos).removeRecursively());
  // An unset XDG directory resolves to the home folder itself.
  where.music = where.home;
  DesktopPlacesController controller(where, QStringList{m_program});
  QVariantMap all;
  for (const QString &placeId : DesktopPlacesController::placeIds()) {
    all.insert(DesktopPlacesController::settingKey(placeId), true);
  }
  controller.setSettings(all);
  const QStringList ids = placeIdsOf(controller.rows());
  QVERIFY(!ids.contains(QStringLiteral("videos")));
  QVERIFY(!ids.contains(QStringLiteral("music")));
  QVERIFY(ids.contains(QStringLiteral("home")));
  // The Trash shows before anything has ever been trashed.
  QVERIFY(ids.contains(QStringLiteral("trash")));
}

void DesktopPlacesControllerTests::rowsCarryStandardIconNamesAndAccessibleNames() {
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QVariantMap all;
  for (const QString &placeId : DesktopPlacesController::placeIds()) {
    all.insert(DesktopPlacesController::settingKey(placeId), true);
  }
  controller.setSettings(all);
  const QHash<QString, QString> icons{
      {QStringLiteral("computer"), QStringLiteral("computer")},
      {QStringLiteral("home"), QStringLiteral("user-home")},
      {QStringLiteral("documents"), QStringLiteral("folder-documents")},
      {QStringLiteral("downloads"), QStringLiteral("folder-download")},
      {QStringLiteral("pictures"), QStringLiteral("folder-pictures")},
      {QStringLiteral("videos"), QStringLiteral("folder-videos")},
      {QStringLiteral("music"), QStringLiteral("folder-music")},
      {QStringLiteral("trash"), QStringLiteral("user-trash")}};
  for (auto icon = icons.cbegin(); icon != icons.cend(); ++icon) {
    const QVariantMap row = rowFor(controller.rows(), icon.key());
    QCOMPARE(row.value(QStringLiteral("iconName")).toString(), icon.value());
    QCOMPARE(row.value(QStringLiteral("id")).toString(), QStringLiteral("place:") + icon.key());
    QCOMPARE(row.value(QStringLiteral("layoutKey")), row.value(QStringLiteral("id")));
    QVERIFY(row.value(QStringLiteral("isPlace")).toBool());
    QVERIFY(!row.value(QStringLiteral("accessibleName")).toString().isEmpty());
  }
  QCOMPARE(rowFor(controller.rows(), QStringLiteral("trash"))
               .value(QStringLiteral("accessibleName"))
               .toString(),
           QStringLiteral("Trash, empty"));
}

void DesktopPlacesControllerTests::trashIconFollowsItsContents() {
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QVERIFY(!controller.trashFull());
  QVERIFY(QDir().mkpath(locations().trashFiles));
  QVERIFY(writeFile(QDir(locations().trashFiles).filePath(QStringLiteral("old.txt"))));
  controller.refresh();
  QVERIFY(controller.trashFull());
  const QVariantMap trash = rowFor(controller.rows(), QStringLiteral("trash"));
  QCOMPARE(trash.value(QStringLiteral("iconName")).toString(), QStringLiteral("user-trash-full"));
  QCOMPARE(trash.value(QStringLiteral("accessibleName")).toString(),
           QStringLiteral("Trash, contains items"));
  // The watcher notices it emptying without an explicit refresh.
  QVERIFY(QFile::remove(QDir(locations().trashFiles).filePath(QStringLiteral("old.txt"))));
  QTRY_VERIFY_WITH_TIMEOUT(!controller.trashFull(), 5000);
}

void DesktopPlacesControllerTests::openingAPlaceStartsFileManagerOnItsFolder() {
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QVERIFY2(controller.open(QStringLiteral("place:home")), qPrintable(controller.feedback()));
  const QString canonical = QFileInfo(m_home->path()).canonicalFilePath();
  QTRY_COMPARE(recorded(m_record), canonical.toLocal8Bit() + '\0');
}

void DesktopPlacesControllerTests::openingAnEmptyTrashCreatesItFirst() {
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QVERIFY(!QFileInfo(locations().trashFiles).exists());
  QVERIFY2(controller.open(QStringLiteral("place:trash")), qPrintable(controller.feedback()));
  QVERIFY(QFileInfo(locations().trashFiles).isDir());
  QVERIFY(QFileInfo(m_data->filePath(QStringLiteral("Trash/info"))).isDir());
  const QString canonical = QFileInfo(locations().trashFiles).canonicalFilePath();
  QTRY_COMPARE(recorded(m_record), canonical.toLocal8Bit() + '\0');
}

void DesktopPlacesControllerTests::openRefusesAnythingThatIsNotAShownPlace() {
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QVERIFY(!controller.open(QStringLiteral("place:documents"))); // not enabled
  QVERIFY(!controller.feedback().isEmpty());
  QVERIFY(!controller.open(m_home->filePath(QStringLiteral("Documents"))));
  QVERIFY(!controller.isPlace(m_home->filePath(QStringLiteral("Documents"))));
  QVERIFY(controller.isPlace(QStringLiteral("place:home")));
  QVERIFY(recorded(m_record).isEmpty());
}

void DesktopPlacesControllerTests::droppedFilesMoveToTheTrashNeverDeleted() {
  const QString dropped = m_home->filePath(QStringLiteral("Downloads/report.txt"));
  QVERIFY(writeFile(dropped));
  DesktopPlacesController controller(locations(), QStringList{m_program});
  QVERIFY2(controller.trashUrls({QUrl::fromLocalFile(dropped)}),
           qPrintable(controller.feedback()));
  QTRY_VERIFY_WITH_TIMEOUT(
      QFileInfo::exists(QDir(locations().trashFiles).filePath(QStringLiteral("report.txt"))),
      5000);
  QVERIFY(!QFileInfo::exists(dropped));
  QTRY_VERIFY_WITH_TIMEOUT(controller.trashFull(), 5000);
}

void DesktopPlacesControllerTests::dropsFromInsideTheTrashOrNotLocalAreRefused() {
  QVERIFY(QDir().mkpath(locations().trashFiles));
  const QString inTrash = QDir(locations().trashFiles).filePath(QStringLiteral("gone.txt"));
  QVERIFY(writeFile(inTrash));
  const QString keep = m_home->filePath(QStringLiteral("keep.txt"));
  QVERIFY(writeFile(keep));
  DesktopPlacesController controller(locations(), QStringList{m_program});
  // One bad item refuses the whole drop before anything moves.
  QVERIFY(!controller.trashUrls({QUrl::fromLocalFile(keep), QUrl::fromLocalFile(inTrash)}));
  QVERIFY(!controller.feedback().isEmpty());
  QVERIFY(!controller.trashUrls({QUrl(QStringLiteral("sftp://host/file.txt"))}));
  QVERIFY(!controller.trashUrls(
      {QUrl::fromLocalFile(m_home->filePath(QStringLiteral("never-existed.txt")))}));
  QVERIFY(QFileInfo::exists(keep));
  QVERIFY(QFileInfo::exists(inTrash));
}

// The applet-settings path is the only storage: every place's switch must be
// declared in the desktop-icons manifest with the same default.
void DesktopPlacesControllerTests::manifestDeclaresEverySettingWithItsDefault() {
  QFile manifest(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets/desktop-icons.json"));
  QVERIFY(manifest.open(QIODevice::ReadOnly));
  const QJsonObject properties = QJsonDocument::fromJson(manifest.readAll())
                                     .object()
                                     .value(QStringLiteral("settingsSchema"))
                                     .toObject()
                                     .value(QStringLiteral("properties"))
                                     .toObject();
  for (const QString &placeId : DesktopPlacesController::placeIds()) {
    const QJsonObject property =
        properties.value(DesktopPlacesController::settingKey(placeId)).toObject();
    QCOMPARE(property.value(QStringLiteral("type")).toString(), QStringLiteral("boolean"));
    QCOMPARE(property.value(QStringLiteral("default")).toBool(),
             DesktopPlacesController::shownByDefault(placeId));
    QVERIFY(!property.value(QStringLiteral("title")).toString().isEmpty());
  }
}

QTEST_GUILESS_MAIN(DesktopPlacesControllerTests)
#include "tst_desktop_places_controller.moc"
