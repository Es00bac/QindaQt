// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "src/apps/settings_center/settings_navigation_controller.h"

#include <QtGui/QAccessible>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

namespace QindaQt::Apps::SettingsPower::TestSupport {

inline QQuickItem *powerSceneItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = powerSceneItem(child, name)) return found;
  return nullptr;
}

inline void verifyCompactPowerNavigation(
    QQuickWindow &window,
    SettingsCenter::SettingsNavigationController &navigation) {
  QTest::keyClick(&window, Qt::Key_8, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("power"));
  auto *loader = powerSceneItem(window.contentItem(),
      QStringLiteral("compactSettingsRoutePowerLoader"));
  auto *tab = powerSceneItem(window.contentItem(),
      QStringLiteral("settingsCompactTab_power"));
  auto *screenLock = powerSceneItem(window.contentItem(),
      QStringLiteral("powerAutomaticScreenLock"));
  QVERIFY(loader != nullptr);
  QVERIFY(loader->property("active").toBool());
  QVERIFY(tab != nullptr);
  QVERIFY(screenLock != nullptr);
  QVERIFY(screenLock->isEnabled());
  auto *screenLockAccessible = QAccessible::queryAccessibleInterface(screenLock);
  QVERIFY(screenLockAccessible != nullptr);
  QCOMPARE(screenLockAccessible->role(), QAccessible::CheckBox);
  QVERIFY(screenLockAccessible->state().checkable);
  QTest::keyClick(&window, Qt::Key_Escape);
  QTRY_COMPARE(window.activeFocusItem(), tab);
  QTest::keyClick(&window, Qt::Key_Tab);
  QTRY_COMPARE(window.activeFocusItem(), screenLock);
}

inline void verifyWidePowerNavigation(
    QQuickWindow &window,
    SettingsCenter::SettingsNavigationController &navigation) {
  QTest::keyClick(&window, Qt::Key_8, Qt::ControlModifier);
  QCOMPARE(navigation.activeRouteId(), QStringLiteral("power"));
  auto *loader = powerSceneItem(window.contentItem(),
      QStringLiteral("wideSettingsRoutePowerLoader"));
  auto *screenLock = powerSceneItem(window.contentItem(),
      QStringLiteral("powerAutomaticScreenLock"));
  auto *tab = powerSceneItem(window.contentItem(),
      QStringLiteral("settingsNavButton_power"));
  QVERIFY(loader != nullptr);
  QVERIFY(loader->property("active").toBool());
  QVERIFY(screenLock != nullptr);
  QVERIFY(screenLock->isEnabled());
  auto *screenLockAccessible = QAccessible::queryAccessibleInterface(screenLock);
  QVERIFY(screenLockAccessible != nullptr);
  QCOMPARE(screenLockAccessible->role(), QAccessible::CheckBox);
  QVERIFY(screenLockAccessible->state().checkable);
  QVERIFY(tab != nullptr);
  auto *accessible = QAccessible::queryAccessibleInterface(tab);
  QVERIFY(accessible != nullptr);
  QCOMPARE(accessible->role(), QAccessible::PageTab);
  QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Power"));
  QVERIFY(accessible->state().selected);
  QTest::keyClick(&window, Qt::Key_Escape);
  QTRY_COMPARE(window.activeFocusItem(), tab);
  QTest::keyClick(&window, Qt::Key_Tab);
  QTRY_COMPARE(window.activeFocusItem(), screenLock);
}

} // namespace QindaQt::Apps::SettingsPower::TestSupport
