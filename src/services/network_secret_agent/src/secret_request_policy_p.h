// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <optional>

namespace QindaQt::Network::SecretAgent::Private {

inline constexpr quint32 kAllowInteraction = 0x1U;
inline constexpr quint32 kKnownGetSecretsFlags = 0x0fU;
inline constexpr quint32 kSecretFlagNone = 0U;
inline constexpr quint32 kSecretFlagNotSaved = 0x2U;

[[nodiscard]] std::optional<PromptRequest>
promptFor(const GetSecretsRequest &request, quint64 requestId);
[[nodiscard]] SecretReply replyFor(const PromptRequest &request,
                                   PromptResult &result);
[[nodiscard]] QString flagsKey(const QString &key);

} // namespace QindaQt::Network::SecretAgent::Private
