// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <optional>
namespace QindaQt::Session::NativeSleep {
// Bounded public intent. No caller-selected method name reaches logind.
enum class SleepMode { Suspend, Hibernate, HybridSleep, SuspendThenHibernate };
inline QString actionMethod(SleepMode mode) {
  switch (mode) {
  case SleepMode::Suspend: return QStringLiteral("Suspend");
  case SleepMode::Hibernate: return QStringLiteral("Hibernate");
  case SleepMode::HybridSleep: return QStringLiteral("HybridSleep");
  case SleepMode::SuspendThenHibernate: return QStringLiteral("SuspendThenHibernate");
  }
  return {};
}
inline QString capabilityMethod(SleepMode mode) {
  const auto action = actionMethod(mode);
  return action.isEmpty() ? QString{} : QStringLiteral("Can") + action;
}
inline std::optional<SleepMode> methodMode(const QString &method, bool capability) {
  for (const auto mode : {SleepMode::Suspend, SleepMode::Hibernate,
                         SleepMode::HybridSleep, SleepMode::SuspendThenHibernate}) {
    if (method == (capability ? capabilityMethod(mode) : actionMethod(mode))) return mode;
  }
  return std::nullopt;
}
}
