// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/applet/keyed_row_model.h"

#include <QAbstractItemModelTester>
#include <QPersistentModelIndex>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QtTest>

#include <memory>

using QindaQt::ShellTaskListApplet::KeyedRowModel;

namespace {
QVariant row(const QString &id, int revision = 0) {
  return QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("revision"), revision}};
}
}

class KeyedRowModelTests final : public QObject {
  Q_OBJECT
private slots:
  void diffPreservesPersistentIdentity();
  void invalidSnapshotsClearPresentation();
  void repeatedSnapshotsRetainDelegates();
  void snapshotFieldsNeverCountAsRowChanges();
  void aTitleChangeTouchesExactlyOneRowAndRole();
  void identityRolesAreNeverVolatile();
};

void KeyedRowModelTests::diffPreservesPersistentIdentity() {
  KeyedRowModel model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.setIdentityRoles({QStringLiteral("id")});
  model.setSourceRows({row(QStringLiteral("a")), row(QStringLiteral("b")), row(QStringLiteral("c"))});
  const QPersistentModelIndex a(model.index(0));
  const QPersistentModelIndex b(model.index(1));
  QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
  QSignalSpy inserts(&model, &QAbstractItemModel::rowsInserted);
  QSignalSpy removes(&model, &QAbstractItemModel::rowsRemoved);
  QSignalSpy moves(&model, &QAbstractItemModel::rowsMoved);
  QSignalSpy updates(&model, &QAbstractItemModel::dataChanged);
  model.setSourceRows({row(QStringLiteral("c")), row(QStringLiteral("a"), 4), row(QStringLiteral("d"))});
  QVERIFY(a.isValid());
  QCOMPARE(a.row(), 1);
  QVERIFY(!b.isValid());
  QCOMPARE(a.data(Qt::UserRole + 1).toMap().value(QStringLiteral("revision")).toInt(), 4);
  QCOMPARE(resets.size(), 0);
  QCOMPARE(inserts.size(), 1);
  QCOMPARE(removes.size(), 1);
  QCOMPARE(moves.size(), 1);
  QCOMPARE(updates.size(), 1);
  model.setSourceRows(model.sourceRows());
  QCOMPARE(updates.size(), 1);
}

void KeyedRowModelTests::invalidSnapshotsClearPresentation() {
  KeyedRowModel model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.setIdentityRoles({QStringLiteral("id")});
  model.setSourceRows({row(QStringLiteral("a"))});
  model.setSourceRows({row(QStringLiteral("a")), row(QStringLiteral("a"))});
  QCOMPARE(model.rowCount(), 0);
  model.setSourceRows({row(QStringLiteral("a"))});
  model.setSourceRows({row(QString{})});
  QCOMPARE(model.rowCount(), 0);
  QVariantList excess;
  for (int i = 0; i < 4097; ++i)
    excess.append(row(QString::number(i)));
  model.setSourceRows(excess);
  QVERIFY(model.sourceRows().isEmpty());
  QCOMPARE(model.rowCount(), 0);
}

void KeyedRowModelTests::repeatedSnapshotsRetainDelegates() {
  KeyedRowModel model;
  model.setIdentityRoles({QStringLiteral("id")});
  QVariantList rows;
  for (int i = 0; i < 64; ++i)
    rows.append(row(QString::number(i)));
  model.setSourceRows(rows);
  QQmlEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("rowModel"), &model);
  QQmlComponent component(&engine);
  component.setData(R"QML(
    import QtQuick
    Item {
      id: root
      property int created: 0
      property int removed: 0
      Repeater {
        model: rowModel
        delegate: Item {
          required property var rowData
          objectName: rowData.id
          property int revision: rowData.revision
          Component.onCompleted: ++root.created
          Component.onDestruction: ++root.removed
        }
      }
    }
  )QML", QUrl(QStringLiteral("inmemory:retained-delegates.qml")));
  QTRY_VERIFY(component.status() != QQmlComponent::Loading);
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  std::unique_ptr<QObject> owned(component.create());
  QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
  auto *root = qobject_cast<QQuickItem *>(owned.get());
  QVERIFY(root != nullptr);
  QHash<QString, QQuickItem *> original;
  for (QQuickItem *child : root->childItems())
    if (!child->objectName().isEmpty())
      original.insert(child->objectName(), child);
  QCOMPARE(original.size(), 64);
  QCOMPARE(root->property("created").toInt(), 64);
  for (int revision = 1; revision <= 1000; ++revision) {
    for (int i = 0; i < rows.size(); ++i) {
      QVariantMap map = rows.at(i).toMap();
      map.insert(QStringLiteral("revision"), revision);
      rows[i] = map;
    }
    model.setSourceRows(rows);
  }
  QCOMPARE(root->property("created").toInt(), 64);
  QCOMPARE(root->property("removed").toInt(), 0);
  for (QQuickItem *child : root->childItems()) {
    if (child->objectName().isEmpty())
      continue;
    QCOMPARE(child, original.value(child->objectName()));
    QCOMPARE(child->property("revision").toInt(), 1000);
  }
  rows.move(63, 0);
  rows.insert(10, row(QStringLiteral("new")));
  rows.removeAt(20);
  model.setSourceRows(rows);
  QCOMPARE(root->property("created").toInt(), 65);
  // Repeater may defer QObject deletion; absence from the visual tree is the
  // immediate removal guarantee required for safe intent targets.
  int survivors = 0;
  for (QQuickItem *child : root->childItems()) {
    if (original.contains(child->objectName())) {
      QCOMPARE(child, original.value(child->objectName()));
      ++survivors;
    }
  }
  QCOMPARE(survivors, 63);
}

namespace {
// A task-list-shaped row: `generationRevision` describes the snapshot every
// row was projected from, `title` and `active` describe the row.
QVariant taskRow(const QString &id, const QString &title, quint64 generation,
                 bool active = false) {
  return QVariantMap{{QStringLiteral("taskId"), id},
                     {QStringLiteral("title"), title},
                     {QStringLiteral("active"), active},
                     {QStringLiteral("generationRevision"), generation}};
}

QVariantList taskRows(int count, quint64 generation,
                      const QString &spinnerTitle = QStringLiteral("idle")) {
  QVariantList rows;
  for (int index = 0; index < count; ++index) {
    rows.append(taskRow(QStringLiteral("w%1").arg(index),
                        index == 0 ? spinnerTitle : QStringLiteral("Window %1").arg(index),
                        generation));
  }
  return rows;
}
} // namespace

void KeyedRowModelTests::snapshotFieldsNeverCountAsRowChanges() {
  KeyedRowModel model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.setIdentityRoles({QStringLiteral("taskId")});
  model.setVolatileRoles({QStringLiteral("generationRevision")});
  QCOMPARE(model.volatileRoles(), QStringList{QStringLiteral("generationRevision")});
  model.setSourceRows(taskRows(12, 1));
  QCOMPARE(model.rowCount(), 12);
  // The published row has no snapshot field: a retained delegate cannot be
  // handed a revision that the next publication will already have replaced.
  for (int index = 0; index < model.rowCount(); ++index) {
    const QVariantMap published = model.index(index).data(Qt::UserRole + 1).toMap();
    QVERIFY(!published.contains(QStringLiteral("generationRevision")));
    QVERIFY(published.contains(QStringLiteral("title")));
  }

  QSignalSpy updates(&model, &QAbstractItemModel::dataChanged);
  QSignalSpy structure(&model, &QAbstractItemModel::rowsInserted);
  QSignalSpy removals(&model, &QAbstractItemModel::rowsRemoved);
  QSignalSpy moves(&model, &QAbstractItemModel::rowsMoved);
  // Fifty publications that change nothing but the snapshot revision.
  for (quint64 generation = 2; generation < 52; ++generation) {
    model.setSourceRows(taskRows(12, generation));
  }
  QCOMPARE(updates.size(), 0);
  QCOMPARE(structure.size(), 0);
  QCOMPARE(removals.size(), 0);
  QCOMPARE(moves.size(), 0);
}

void KeyedRowModelTests::aTitleChangeTouchesExactlyOneRowAndRole() {
  KeyedRowModel model;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
  model.setIdentityRoles({QStringLiteral("taskId")});
  model.setVolatileRoles({QStringLiteral("generationRevision")});
  model.setSourceRows(taskRows(16, 1));
  QSignalSpy updates(&model, &QAbstractItemModel::dataChanged);

  // The title-spinner case: row 0 changes title on every publication while
  // the snapshot revision advances for all sixteen rows.
  constexpr int publications = 200;
  for (int generation = 0; generation < publications; ++generation) {
    model.setSourceRows(taskRows(16, quint64(generation) + 2,
                                 QStringLiteral("spinner %1").arg(generation)));
  }
  QCOMPARE(updates.size(), publications);
  for (const QList<QVariant> &arguments : updates) {
    const QModelIndex first = arguments.at(0).value<QModelIndex>();
    const QModelIndex last = arguments.at(1).value<QModelIndex>();
    QCOMPARE(first.row(), 0);
    QCOMPARE(last.row(), 0);
    QCOMPARE(arguments.at(2).value<QList<int>>(), QList<int>{Qt::UserRole + 1});
  }
  QCOMPARE(model.index(0).data(Qt::UserRole + 1).toMap().value(QStringLiteral("title")).toString(),
           QStringLiteral("spinner %1").arg(publications - 1));
  // A different field on a different row is likewise exactly one update.
  updates.clear();
  QVariantList rows = taskRows(16, 999, QStringLiteral("spinner %1").arg(publications - 1));
  rows[7] = taskRow(QStringLiteral("w7"), QStringLiteral("Window 7"), 999, true);
  model.setSourceRows(rows);
  QCOMPARE(updates.size(), 1);
  QCOMPARE(updates.constFirst().at(0).value<QModelIndex>().row(), 7);
}

void KeyedRowModelTests::identityRolesAreNeverVolatile() {
  KeyedRowModel model;
  model.setIdentityRoles({QStringLiteral("taskId")});
  // A hostile or mistaken configuration naming the identity as volatile must
  // not strip it: the delegate would lose the very key that retains it.
  model.setVolatileRoles({QStringLiteral("taskId"), QStringLiteral("generationRevision")});
  model.setSourceRows(taskRows(3, 1));
  const QVariantMap published = model.index(1).data(Qt::UserRole + 1).toMap();
  QCOMPARE(published.value(QStringLiteral("taskId")).toString(), QStringLiteral("w1"));
  QVERIFY(!published.contains(QStringLiteral("generationRevision")));
  // The bound mirrors identityRoles: more than eight volatile names clear it.
  model.setVolatileRoles(QStringList(9, QStringLiteral("x")));
  QVERIFY(model.volatileRoles().isEmpty());
}

QTEST_MAIN(KeyedRowModelTests)
#include "tst_keyed_row_model.moc"
