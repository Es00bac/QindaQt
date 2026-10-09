// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace QindaQt::ShellLauncher {

enum class ApplicationOrigin { Native, Android, Windows };

// Owning, transport-free catalog routing value. This is a registration, not
// live window/process evidence or permission. A runtime consumer must recheck
// the exact current revision and recipe digest before accepting a launch.
struct ManagedApplicationBinding {
  ApplicationOrigin origin = ApplicationOrigin::Native;
  QString registrationId;
  quint64 registrationRevision = 0;
  QString recipeDigest;

  friend bool operator==(const ManagedApplicationBinding &,
                         const ManagedApplicationBinding &) = default;
};

struct ManagedApplicationRegistration {
  ManagedApplicationBinding binding;
  QString name;
  QString genericName;
  QString comment;
  QString iconName;
  QStringList categories;
  QStringList keywords;

  friend bool operator==(const ManagedApplicationRegistration &,
                         const ManagedApplicationRegistration &) = default;
};

// Positive revision, Android/Windows origin, canonical non-null lowercase
// UUID and lowercase SHA-256. No filesystem, transport or authentication is
// performed here. An arbitrary desktop key never creates this binding.
[[nodiscard]] bool isValidManagedApplicationBinding(
    const ManagedApplicationBinding &binding);

// Stable across relaunch, recipe updates and display-name changes. Returns
// empty for malformed input; recipe/revision are deliberately not in the ID.
[[nodiscard]] QString managedApplicationEntryId(
    const ManagedApplicationBinding &binding);

// Validates a complete supplied snapshot atomically (<=4096 rows). Copies own
// all data and validation is reentrant. Error text is bounded/static and never
// includes an untrusted label. It does not authenticate the producer.
[[nodiscard]] bool validateManagedApplications(
    const QVector<ManagedApplicationRegistration> &registrations,
    QString *error = nullptr);

} // namespace QindaQt::ShellLauncher
