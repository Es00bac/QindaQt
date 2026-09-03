// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "src/apps/settings_center/settings_navigation_controller.h"

#include <QtGui/QAccessible>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

namespace QindaQt::Apps::SettingsClipboard::TestSupport {

inline QQuickItem *findClipboardSceneItem(QQuickItem *root,
                                          const QString &objectName)
{
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == objectName) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *match = findClipboardSceneItem(child, objectName)) {
            return match;
        }
    }
    return nullptr;
}

inline void verifyClipboardRouteInHost(
    QQuickWindow &window,
    QindaQt::Apps::SettingsCenter::SettingsNavigationController &navigation,
    bool compact)
{
    QTest::keyClick(&window, Qt::Key_9, Qt::ControlModifier);
    QCOMPARE(navigation.activeRouteId(), QStringLiteral("clipboard"));
    const QString prefix = compact ? QStringLiteral("compactSettingsRoute")
                                   : QStringLiteral("wideSettingsRoute");
    auto *loader = findClipboardSceneItem(
        window.contentItem(), prefix + QStringLiteral("ClipboardLoader"));
    QVERIFY(loader != nullptr);
    QCOMPARE(loader->property("active").toBool(), true);

    const QString tabName = compact
        ? QStringLiteral("settingsCompactTab_clipboard")
        : QStringLiteral("settingsNavButton_clipboard");
    auto *tab = findClipboardSceneItem(window.contentItem(), tabName);
    auto *close = findClipboardSceneItem(
        window.contentItem(), QStringLiteral("clipboardCloseButton"));
    QVERIFY(tab != nullptr);
    QVERIFY(close != nullptr);
    QVERIFY(close->isEnabled());
    auto *accessible = QAccessible::queryAccessibleInterface(tab);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->role(), QAccessible::PageTab);
    QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Clipboard"));
    QVERIFY(accessible->state().selected);

    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_COMPARE(window.activeFocusItem(), tab);
    QTest::keyClick(&window, Qt::Key_Tab);
    QTRY_COMPARE(window.activeFocusItem(), close);
}

} // namespace QindaQt::Apps::SettingsClipboard::TestSupport
