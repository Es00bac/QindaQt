// SPDX-License-Identifier: GPL-3.0-or-later
#include "applications_place_order.h"

#include "navigation_controller.h"

namespace QindaQt::Apps::FileManager {

ApplicationsPlaceOrder::ApplicationsPlaceOrder(NavigationController &navigation,
                                               QObject *parent)
    : QObject(parent), m_navigation(navigation) {
  connect(&navigation, &NavigationController::navigationChanged, this,
          &ApplicationsPlaceOrder::onNavigationChanged);
  onNavigationChanged();
}

void ApplicationsPlaceOrder::onNavigationChanged() {
  const bool inPlace = m_navigation.applicationsPlace();
  if (inPlace == m_inPlace) {
    return;
  }
  m_inPlace = inPlace;
  if (inPlace) {
    m_folderOrder = current();
    apply(m_placeOrder);
  } else {
    m_placeOrder = current();
    apply(m_folderOrder);
  }
}

ApplicationsPlaceOrder::Order ApplicationsPlaceOrder::current() const {
  return {m_navigation.sortColumn(), m_navigation.sortDirection()};
}

void ApplicationsPlaceOrder::apply(const Order &order) {
  // AGENT-GUARD: setSortColumn() flips the direction when given the active
  // column (the header-click rule), so each call is skipped unless it moves
  // toward the wanted order -- the same idempotence FolderViewSettings.qml
  // keeps. A column change lands on ascending; one more call makes it
  // descending.
  if (order.column.isEmpty()) {
    return;
  }
  if (m_navigation.sortColumn() != order.column) {
    m_navigation.setSortColumn(order.column);
  }
  if (m_navigation.sortDirection() != order.direction) {
    m_navigation.setSortColumn(order.column);
  }
}

} // namespace QindaQt::Apps::FileManager
