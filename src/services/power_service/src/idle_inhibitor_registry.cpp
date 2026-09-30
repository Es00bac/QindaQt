// SPDX-License-Identifier: GPL-3.0-or-later
#include "idle_inhibitor_registry_p.h"

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QChar>
#include <QtCore/QUuid>

namespace QindaQt::Power {
namespace {

constexpr quint32 KnownScopes =
    static_cast<quint32>(IdleInhibitorScope::AutomaticLock) |
    static_cast<quint32>(IdleInhibitorScope::DisplayOff) |
    static_cast<quint32>(IdleInhibitorScope::IdleSuspend);

bool safeBoundedText(const QString &text, const qsizetype maximumBytes) {
  if (text.isEmpty() || text.toUtf8().size() > maximumBytes) {
    return false;
  }
  for (const QChar character : text) {
    if (character.category() == QChar::Other_Control ||
        character.category() == QChar::Other_Format) {
      return false;
    }
  }
  return true;
}

bool validUniqueOwner(const QString &owner) {
  return owner.startsWith(QLatin1Char(':')) &&
         safeBoundedText(owner, kMaxNameUtf8Bytes);
}

bool validScopes(const IdleInhibitorScopes scopes) {
  const quint32 raw = static_cast<quint32>(scopes.toInt());
  return raw != 0 && (raw & ~KnownScopes) == 0;
}

} // namespace

IdleInhibitorRegistry::IdleInhibitorRegistry(
    const quint64 epoch, const IdleInhibitorScopes consumedScopes)
    : m_epoch(epoch), m_consumedScopes(consumedScopes) {
  if ((static_cast<quint32>(m_consumedScopes.toInt()) & ~KnownScopes) != 0) {
    m_consumedScopes = {};
  }
}

void IdleInhibitorRegistry::setEpoch(const quint64 epoch) {
  if (epoch == m_epoch) {
    return;
  }
  m_epoch = epoch;
  m_leases.clear();
}

void IdleInhibitorRegistry::setConsumedScopes(
    const IdleInhibitorScopes scopes) {
  const quint32 raw = static_cast<quint32>(scopes.toInt());
  m_consumedScopes = (raw & ~KnownScopes) == 0 ? scopes : IdleInhibitorScopes{};
  for (auto it = m_leases.begin(); it != m_leases.end();) {
    if ((it->scopes & ~m_consumedScopes) != IdleInhibitorScopes{}) {
      it = m_leases.erase(it);
    } else {
      ++it;
    }
  }
}

IdleInhibitorScopes IdleInhibitorRegistry::consumedScopes() const noexcept {
  return m_consumedScopes;
}

IdleInhibitorAcquireResult IdleInhibitorRegistry::acquire(
    const QString &uniqueOwner, const QString &application,
    const QString &reason, const IdleInhibitorScopes scopes) {
  if (m_epoch == 0 || !validUniqueOwner(uniqueOwner) ||
      !safeBoundedText(application, kMaxNameUtf8Bytes) ||
      !safeBoundedText(reason, kMaxInhibitorWhyUtf8Bytes) ||
      !validScopes(scopes)) {
    return {.status = IdleInhibitorAcquireStatus::Invalid,
            .handle = {},
            .reasonCode = QStringLiteral("invalid-inhibitor-request")};
  }
  if ((scopes & ~m_consumedScopes) != IdleInhibitorScopes{}) {
    return {.status = IdleInhibitorAcquireStatus::Unsupported,
            .handle = {},
            .reasonCode = QStringLiteral("inhibitor-scope-unsupported")};
  }

  qsizetype ownerCount = 0;
  for (auto it = m_leases.cbegin(); it != m_leases.cend(); ++it) {
    if (it->uniqueOwner == uniqueOwner) {
      ++ownerCount;
    }
  }
  if (m_leases.size() >= MaxLeases || ownerCount >= MaxLeasesPerOwner) {
    return {.status = IdleInhibitorAcquireStatus::Capacity,
            .handle = {},
            .reasonCode = QStringLiteral("inhibitor-capacity-reached")};
  }

  QString opaqueId;
  do {
    opaqueId = QUuid::createUuid().toString(QUuid::Id128);
  } while (m_leases.contains(opaqueId));
  m_leases.insert(opaqueId,
                  Lease{.uniqueOwner = uniqueOwner, .scopes = scopes});
  return {.status = IdleInhibitorAcquireStatus::Accepted,
          .handle = Handle{.epoch = m_epoch, .opaqueId = opaqueId},
          .reasonCode = QStringLiteral("accepted")};
}

bool IdleInhibitorRegistry::release(const QString &uniqueOwner,
                                    const Handle &handle) {
  if (m_epoch == 0 || handle.epoch != m_epoch || handle.opaqueId.isEmpty()) {
    return false;
  }
  const auto it = m_leases.find(handle.opaqueId);
  if (it == m_leases.end() || it->uniqueOwner != uniqueOwner) {
    return false;
  }
  m_leases.erase(it);
  return true;
}

void IdleInhibitorRegistry::ownerVanished(const QString &uniqueOwner) {
  if (uniqueOwner.isEmpty()) {
    return;
  }
  for (auto it = m_leases.begin(); it != m_leases.end();) {
    if (it->uniqueOwner == uniqueOwner) {
      it = m_leases.erase(it);
    } else {
      ++it;
    }
  }
}

bool IdleInhibitorRegistry::isInhibited(
    const IdleInhibitorScope scope) const noexcept {
  for (auto it = m_leases.cbegin(); it != m_leases.cend(); ++it) {
    if (it->scopes.testFlag(scope)) {
      return true;
    }
  }
  return false;
}

qsizetype IdleInhibitorRegistry::leaseCount() const noexcept {
  return m_leases.size();
}

} // namespace QindaQt::Power
