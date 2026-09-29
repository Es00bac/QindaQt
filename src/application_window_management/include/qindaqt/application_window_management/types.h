// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QtTypes>
namespace QindaQt::ApplicationWindowManagement {
// Wire values are version-1 protocol constants; never renumber them.
enum class Placement : quint32 { Tab, TileRight, TileDown, TileLeft, TileUp };
enum class Status : quint32 {
  Accepted,
  Denied,
  Invalid,
  Unavailable,
  Timeout,
  Cancelled,
  ResourceLimit
};
} // namespace QindaQt::ApplicationWindowManagement
