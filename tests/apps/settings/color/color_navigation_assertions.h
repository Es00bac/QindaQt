// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "src/apps/settings_center/settings_navigation_controller.h"

#include <QtGui/QAccessible>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

namespace QindaQt::Apps::SettingsColor::TestSupport {

inline QQuickItem *colorSceneItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = colorSceneItem(child, name)) return found;
  return nullptr;
}

inline void verifyCompactColorNavigation(
    QQuickWindow &window,
    SettingsCenter::SettingsNavigationController &navigation) {
  QTest::keyClick(&window, Qt::Key_0, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("color"));
  auto *loader = colorSceneItem(window.contentItem(),
      QStringLiteral("compactSettingsRouteColorLoader"));
  auto *tab = colorSceneItem(window.contentItem(),
      QStringLiteral("settingsCompactTab_color"));
  auto *profile = colorSceneItem(window.contentItem(),
      QStringLiteral("colorProfile_vendor-srgb"));
  QVERIFY(loader != nullptr);
  QVERIFY(loader->property("active").toBool());
  QVERIFY(tab != nullptr);
  QVERIFY(profile != nullptr);
  QTest::keyClick(&window, Qt::Key_Escape);
  QTRY_COMPARE(window.activeFocusItem(), tab);
  QTest::keyClick(&window, Qt::Key_Tab);
  QTRY_COMPARE(window.activeFocusItem(), profile);
}

inline void verifyWideColorNavigation(
    QQuickWindow &window,
    SettingsCenter::SettingsNavigationController &navigation) {
  QTest::keyClick(&window, Qt::Key_0, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("color"));
  auto *loader = colorSceneItem(window.contentItem(),
      QStringLiteral("wideSettingsRouteColorLoader"));
  auto *profile = colorSceneItem(window.contentItem(),
      QStringLiteral("colorProfile_vendor-srgb"));
  auto *tab = colorSceneItem(window.contentItem(),
      QStringLiteral("settingsNavButton_color"));
  QVERIFY(loader != nullptr);
  QVERIFY(loader->property("active").toBool());
  QVERIFY(profile != nullptr);
  QVERIFY(profile->isEnabled());
  QVERIFY(tab != nullptr);
  auto *accessible = QAccessible::queryAccessibleInterface(tab);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::PageTab);
  QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Color"));
  QVERIFY(accessible->state().selected);
  QTest::keyClick(&window, Qt::Key_Escape);
  QTRY_COMPARE(window.activeFocusItem(), tab);
  QTest::keyClick(&window, Qt::Key_Tab);
  QTRY_COMPARE(window.activeFocusItem(), profile);
}

} // namespace QindaQt::Apps::SettingsColor::TestSupport
