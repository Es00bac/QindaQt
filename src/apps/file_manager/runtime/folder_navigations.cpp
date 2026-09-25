// SPDX-License-Identifier: GPL-3.0-or-later
#include "folder_navigations.h"

#include "../model/navigation_controller.h"

#include <QQmlEngine>

namespace QindaQt::Apps::FileManager {

FolderNavigations::FolderNavigations(NavigationController &first, Recipe recipe,
                                     Binder binder, QObject *parent)
    : QObject(parent), m_first(first), m_recipe(std::move(recipe)),
      m_binder(std::move(binder)) {
  bind(&m_first);
}

FolderNavigations::~FolderNavigations() {
  // Unbind before the child controllers go away with this object.
  m_context.reset();
}

QObject *FolderNavigations::create(const QString &path) {
  std::unique_ptr<NavigationController> made = m_recipe();
  if (!made) {
    return nullptr;
  }
  NavigationController *navigation = made.release();
  navigation->setParent(this);
  // AGENT-GUARD: QML must never garbage-collect a controller its views bind
  // to; the tab's lifetime is release()'s to end.
  QQmlEngine::setObjectOwnership(navigation, QQmlEngine::CppOwnership);
  navigation->navigateTo(path);
  return navigation;
}

void FolderNavigations::release(QObject *navigation) {
  auto *controller = qobject_cast<NavigationController *>(navigation);
  if (controller == nullptr || controller == &m_first || controller->parent() != this) {
    return;
  }
  if (controller == m_active) {
    bind(&m_first);
  }
  controller->deleteLater();
}

void FolderNavigations::setActive(QObject *navigation) {
  auto *controller = qobject_cast<NavigationController *>(navigation);
  if (controller == nullptr || controller == m_active) {
    return;
  }
  bind(controller);
}

QObject *FolderNavigations::active() const { return m_active.data(); }

void FolderNavigations::bind(NavigationController *navigation) {
  // Destroying the old context drops every connection the binder made.
  m_context = std::make_unique<QObject>();
  m_active = navigation;
  m_binder(*navigation, *m_context);
  emit activeChanged();
}

} // namespace QindaQt::Apps::FileManager
