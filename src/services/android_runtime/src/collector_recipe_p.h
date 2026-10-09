// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QCryptographicHash>
namespace QindaQt::Services::AndroidRuntime {
inline QByteArray collectorRecipe() { return QByteArrayLiteral("{\"schema\":1,\"collector\":\"qindaqt-android-collector\",\"runtime\":\"waydroid-stock\",\"protocol\":1,\"guestQualified\":false}\n"); }
inline QByteArray collectorRecipeDigest() { return QCryptographicHash::hash(collectorRecipe(),QCryptographicHash::Sha256).toHex(); }
// Fixed private inherited slots, not a public command-line bootstrap mode.
inline constexpr int collectorSocket=3,collectorRecord=4,collectorExecutable=5,parentLifetime=6,parentExecutable=7;
} // namespace QindaQt::Services::AndroidRuntime
