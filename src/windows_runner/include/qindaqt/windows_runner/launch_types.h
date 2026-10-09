// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::WindowsRunner {

// Copied Windows-only binding. Catalog bridges must first authenticate the
// typed Windows origin; a desktop title, class or Exec string is never a key.
struct ManagedWindowsBinding final {
    QString registrationId;
    quint64 registrationRevision = 0;
    QString recipeDigest;
    friend bool operator==(const ManagedWindowsBinding &,
                           const ManagedWindowsBinding &) = default;
};

struct PayloadPin final {
    QString path;
    QByteArray sha256; // Exactly 32 binary bytes, over the complete payload.
    quint64 bytes = 0;
    friend bool operator==(const PayloadPin &, const PayloadPin &) = default;
};

enum class RuntimeBackend { Wine, ProtonUmu };

struct WindowsRuntimePin final {
    RuntimeBackend backend = RuntimeBackend::Wine;
    PayloadPin launcher;
    // ProtonUmu additionally requires an exact packaged Proton entrypoint and
    // Steam Runtime receipt. No user-directory discovery or rolling alias.
    QString protonDirectory;
    PayloadPin protonEntrypoint;
    PayloadPin steamRuntimeReceipt;
    friend bool operator==(const WindowsRuntimePin &,
                           const WindowsRuntimePin &) = default;
};

// An admitted registration/receipt, resolved privately rather than reconstructed
// from catalog text. The supplying port validates package/recipe provenance and
// per-app profile ownership. No arbitrary installer or download enters here.
struct RegisteredWindowsApplication final {
    ManagedWindowsBinding binding;
    QString packageCpv;
    QString packageRepository;
    QString packageSlot;
    QString profileId;
    QString profileHome;
    QString prefixPath;
    WindowsRuntimePin runtime;
    PayloadPin application;
    QStringList arguments;
    bool materializationReady = false;
    friend bool operator==(const RegisteredWindowsApplication &,
                           const RegisteredWindowsApplication &) = default;
};

struct WindowsLaunchPlan final {
    ManagedWindowsBinding binding;
    QString catalogEntryId;
    QString launchId;
    QString program;
    QStringList arguments;
    QString workingDirectory;
    QProcessEnvironment environment;
    QVector<PayloadPin> payloads;
};

enum class LaunchFailure {
    None,
    InvalidBinding,
    MissingRegistration,
    RegistrationChanged,
    InvalidRecipe,
    ProfileNotReady,
    PayloadChanged,
    DispatchRefused,
    DispatchUncertain,
};

enum class LaunchState { Unavailable, Starting, OutcomeUnknown };

struct WindowsLaunchResult final {
    LaunchState state = LaunchState::Unavailable;
    LaunchFailure failure = LaunchFailure::None;
    QString catalogEntryId;
    QString launchId;
    QString message;
};

[[nodiscard]] bool isValidWindowsBinding(const ManagedWindowsBinding &binding);
// Empty for any invalid binding. Stable across launch incarnations and recipe
// revisions; registrations retain this UUID through an explicit update.
[[nodiscard]] QString windowsCatalogEntryId(const ManagedWindowsBinding &binding);

} // namespace QindaQt::WindowsRunner
