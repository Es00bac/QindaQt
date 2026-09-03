// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickItem>
#include <QString>

namespace QindaQt::Apps::SettingsCenter::TestSupport {

inline QQuickItem *sceneItem(QQuickItem *root, const QString &objectName) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == objectName) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (auto *match = sceneItem(child, objectName); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

inline QObject *sceneObject(QQuickItem *root, const QString &objectName) {
  return sceneItem(root, objectName);
}

} // namespace QindaQt::Apps::SettingsCenter::TestSupport
