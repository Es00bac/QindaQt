// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "windowcontainer.h"
#include <QMap>
#include <QStringList>

namespace QindaQt::Workspaces {
// Owned, copyable values; no compositor handles, processes or ambient services.
// A slot ID is durable. It must never be a live compositor window identity.
struct ApplicationSlot {
  QString id;
  QString label;
  QString desktopEntryId;
  QStringList urls;
};
struct Workspace {
  QString id;
  QString name;
  Core::WindowContainer layout{QStringLiteral("layout")};
  QList<ApplicationSlot> applicationSlots;
  // Empty follows the theme; otherwise an explicit sRGB #RRGGBB accent.
  QString color;
  [[nodiscard]] bool validate(QString *error = nullptr) const;
  [[nodiscard]] QJsonObject toJson() const;
  [[nodiscard]] static std::optional<Workspace>
  fromJson(const QJsonObject &, QString *error = nullptr);
};
// Captures an owned live layout using caller-supplied durable application
// slots. There must be exactly one entry for each live member and no extra
// entries. The caller obtains application intent through its platform
// inventory, not through this module. Failure leaves the source container
// untouched.
[[nodiscard]] std::optional<Workspace>
capture(const QString &id, const QString &name, const QString &color,
        const Core::WindowContainer &liveLayout,
        const QMap<QString, ApplicationSlot> &applicationsByWindow,
        QString *error = nullptr);

struct AvailableWindow {
  QString id;
  QString desktopEntryId;
  bool eligible = true;
};
struct AssignmentPlan {
  QMap<QString, QString> windowsBySlot;
  QStringList unassignedSlots;
  QStringList ambiguousSlots;
  QString error;
  [[nodiscard]] bool complete() const {
    return error.isEmpty() && unassignedSlots.isEmpty();
  }
};
// Explicit assignments may select a different app (e.g. a replacement editor).
// Automatic assignment requires one unassigned slot and one eligible window for
// an app. It never guesses among duplicate terminals or steals grouped windows.
[[nodiscard]] AssignmentPlan
assignWindows(const Workspace &, const QList<AvailableWindow> &,
              const QMap<QString, QString> &explicitAssignments = {});
// Creates a fresh live value only after every slot is assigned exactly once.
// The caller owns uniqueness of containerId in its current topology. No
// mutation is performed here; the compositor must submit the result atomically.
[[nodiscard]] std::optional<Core::WindowContainer>
instantiate(const Workspace &, const AssignmentPlan &,
            const QString &containerId, QString *error = nullptr);
} // namespace QindaQt::Workspaces
