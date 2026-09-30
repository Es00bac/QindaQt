// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QFlags>
#include <QtCore/QHash>
#include <QtCore/QString>

namespace QindaQt::Power {

enum class IdleInhibitorScope : quint32 {
  AutomaticLock = 1U << 0U,
  DisplayOff = 1U << 1U,
  IdleSuspend = 1U << 2U,
};
Q_DECLARE_FLAGS(IdleInhibitorScopes, IdleInhibitorScope)

enum class IdleInhibitorAcquireStatus : quint8 {
  Accepted,
  Unsupported,
  Invalid,
  Capacity,
};

struct IdleInhibitorAcquireResult {
  IdleInhibitorAcquireStatus status = IdleInhibitorAcquireStatus::Invalid;
  Handle handle;
  QString reasonCode;
};

// Power1 process-local lease table. Cookies are useful only with the owning
// D-Bus unique name and current service epoch; callers cannot transfer them.
class IdleInhibitorRegistry final {
public:
  static constexpr qsizetype MaxLeases = 64;
  static constexpr qsizetype MaxLeasesPerOwner = 16;

  explicit IdleInhibitorRegistry(quint64 epoch = 0,
                                 IdleInhibitorScopes consumedScopes = {});

  void setEpoch(quint64 epoch);
  void setConsumedScopes(IdleInhibitorScopes scopes);
  [[nodiscard]] IdleInhibitorScopes consumedScopes() const noexcept;
  [[nodiscard]] IdleInhibitorAcquireResult acquire(const QString &uniqueOwner,
                                                   const QString &application,
                                                   const QString &reason,
                                                   IdleInhibitorScopes scopes);
  [[nodiscard]] bool release(const QString &uniqueOwner, const Handle &handle);
  void ownerVanished(const QString &uniqueOwner);
  [[nodiscard]] bool isInhibited(IdleInhibitorScope scope) const noexcept;
  [[nodiscard]] qsizetype leaseCount() const noexcept;
  [[nodiscard]] qsizetype leaseCountForOwner(const QString &uniqueOwner) const noexcept;

private:
  struct Lease {
    QString uniqueOwner;
    IdleInhibitorScopes scopes;
  };

  quint64 m_epoch = 0;
  IdleInhibitorScopes m_consumedScopes;
  QHash<QString, Lease> m_leases;
};

} // namespace QindaQt::Power

Q_DECLARE_OPERATORS_FOR_FLAGS(QindaQt::Power::IdleInhibitorScopes)
