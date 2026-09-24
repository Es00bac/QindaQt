// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Apps::FileManager {

class NavigationController;

// ADR-0262: the Applications place keeps its own sort, separate from the
// window's folder sort. NavigationController holds one sort for whatever it
// lists; without this, View > Group by Category (the Kind sort there) would
// leak into every folder the window visits afterwards, and a folder sort such
// as Date Modified would reorder the application list.
//
// AGENT-CONTRACT: GUI-thread only; borrows `navigation`, which must outlive
// this object (Qt disconnects if it does not). On entering the place it
// remembers the folder sort and applies the place's (name A to Z until the
// user picks another); on leaving it remembers the place's sort and restores
// the folder one. It only calls NavigationController's public
// setSortColumn(), so the controller learns nothing about applications.
// Construct it before the first navigation so a window that opens straight
// into Applications (--choose-application) starts A to Z.
class ApplicationsPlaceOrder final : public QObject {
public:
  explicit ApplicationsPlaceOrder(NavigationController &navigation,
                                  QObject *parent = nullptr);

private:
  struct Order final {
    QString column;
    QString direction;
  };

  void onNavigationChanged();
  [[nodiscard]] Order current() const;
  void apply(const Order &order);

  NavigationController &m_navigation;
  bool m_inPlace = false;
  Order m_placeOrder{QStringLiteral("name"), QStringLiteral("ascending")};
  Order m_folderOrder;
};

} // namespace QindaQt::Apps::FileManager
