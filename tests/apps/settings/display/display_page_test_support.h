// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>

namespace QindaQt::Tests::DisplayPageSupport {

inline QQuickItem *findItemByObjectName(QQuickItem *root, const QString &name) {
  if (root == nullptr)
    return nullptr;
  if (root->objectName() == name)
    return root;
  for (QQuickItem *child : root->childItems()) {
    if (auto *match = findItemByObjectName(child, name); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

inline QList<QQuickItem *> outputCards(QQuickItem *root) {
  QList<QQuickItem *> cards;
  if (root == nullptr)
    return cards;
  if (root->property("outputData").isValid() &&
      root->property("selected").isValid()) {
    cards.append(root);
  }
  for (QQuickItem *child : root->childItems()) {
    cards.append(outputCards(child));
  }
  return cards;
}

inline void attachPage(QQuickView &view, QQuickItem &page) {
  view.resize(900, 760);
  page.setParentItem(view.contentItem());
  page.setSize(view.size());
  view.show();
  QCoreApplication::processEvents();
}

inline void replaceFocusedText(QQuickView &view, const QString &text) {
  QTest::keyClick(&view, Qt::Key_A, Qt::ControlModifier);
  for (const QChar character : text) {
    if (character == QLatin1Char('-')) {
      QTest::keyClick(&view, Qt::Key_Minus);
      continue;
    }
    QVERIFY(character.isDigit());
    QTest::keyClick(&view, Qt::Key(int(Qt::Key_0) + character.digitValue()));
  }
}

} // namespace QindaQt::Tests::DisplayPageSupport
