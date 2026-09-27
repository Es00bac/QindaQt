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

QTEST_MAIN(KeyedRowModelTests)
#include "tst_keyed_row_model.moc"
