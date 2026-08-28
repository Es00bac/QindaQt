// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_identity/identity_types.h>

#include <QtCore/QByteArrayView>

#include <functional>

namespace QindaQt::DisplayIdentity
{

using DigestFunction = std::function<QByteArray(QByteArrayView)>;

[[nodiscard]] QByteArray truncatedSha256(QByteArrayView value);

// Inputs and the digest callback are borrowed only for the duration of the
// call. The function is thread-safe when the supplied callback is thread-safe.
// It returns values with no raw EDID, serial, or un-hashed private material.
[[nodiscard]] ResolutionResult resolve(const QList<ObservedOutput> &connectedOutputs);
[[nodiscard]] ResolutionResult resolve(const QList<ObservedOutput> &connectedOutputs,
                                       const DigestFunction &digest);

} // namespace QindaQt::DisplayIdentity
