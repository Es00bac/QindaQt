// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/secret_agent_controller.h>

#include "secret_request_policy_p.h"

#include <QtCore/QTimer>

#include <algorithm>
#include <limits>

namespace QindaQt::Network::SecretAgent {

SecretAgentController::SecretAgentController(
    PromptPort &prompt, ConnectionAuthority &authority,
    const int promptTimeoutMilliseconds, QObject *parent)
    : QObject(parent), m_prompt(prompt), m_authority(authority),
      m_promptTimeoutMilliseconds(
          std::clamp(promptTimeoutMilliseconds, 100, 300'000)) {}

SecretAgentController::~SecretAgentController() { cancelAll(); }

bool SecretAgentController::requestSecrets(const GetSecretsRequest &request,
                                           Completion completion) {
  const quint64 requestId = nextRequestId();
  auto prompt = Private::promptFor(request, requestId);
  if (!prompt.has_value() ||
      !m_authority.isKnownConnection(request.connectionPath,
                                     request.networkManagerOwner)) {
    completion(SecretAgentResult::NoSecrets, {});
    return false;
  }
  auto *timer = new QTimer(this);
  timer->setSingleShot(true);
  const PromptRequest promptValue = *prompt;
  m_pending.insert(requestId,
                   Pending{promptValue, std::move(completion), timer});
  connect(timer, &QTimer::timeout, this,
          [this, requestId] { finishCanceled(requestId); });
  if (!m_prompt.showPrompt(promptValue, [this](PromptResult result) {
        finish(std::move(result));
      })) {
    auto pending = m_pending.take(requestId);
    pending.timer->deleteLater();
    pending.completion(SecretAgentResult::NoSecrets, {});
    return false;
  }
  timer->start(m_promptTimeoutMilliseconds);
  return true;
}

void SecretAgentController::cancel(const QString &connectionPath,
                                   const QString &settingName) {
  QList<quint64> matches;
  for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
    if (it->prompt.connectionPath == connectionPath &&
        it->prompt.settingName == settingName) {
      matches.append(it.key());
    }
  }
  for (const quint64 requestId : std::as_const(matches)) {
    finishCanceled(requestId);
  }
}

void SecretAgentController::cancelAll() {
  const QList<quint64> ids = m_pending.keys();
  for (const quint64 requestId : ids) {
    finishCanceled(requestId);
  }
}

qsizetype SecretAgentController::pendingCount() const noexcept {
  return m_pending.size();
}

void SecretAgentController::finish(PromptResult result) {
  auto it = m_pending.find(result.requestId);
  if (it == m_pending.end()) {
    result.wipe();
    return;
  }
  Pending pending = it.value();
  m_pending.erase(it);
  pending.timer->stop();
  pending.timer->deleteLater();
  if (!result.accepted) {
    result.wipe();
    pending.completion(SecretAgentResult::UserCanceled, {});
    return;
  }
  SecretReply reply = Private::replyFor(pending.prompt, result);
  if (reply.values.isEmpty()) {
    pending.completion(SecretAgentResult::NoSecrets, {});
    return;
  }
  pending.completion(SecretAgentResult::Replied, std::move(reply));
}

void SecretAgentController::finishCanceled(const quint64 requestId) {
  auto it = m_pending.find(requestId);
  if (it == m_pending.end()) {
    return;
  }
  Pending pending = it.value();
  m_pending.erase(it);
  pending.timer->stop();
  pending.timer->deleteLater();
  m_prompt.cancelPrompt(requestId);
  pending.completion(SecretAgentResult::UserCanceled, {});
}

quint64 SecretAgentController::nextRequestId() {
  if (m_nextRequestId == std::numeric_limits<quint64>::max()) {
    m_nextRequestId = 0;
  }
  do {
    ++m_nextRequestId;
  } while (m_nextRequestId == 0 || m_pending.contains(m_nextRequestId));
  return m_nextRequestId;
}

} // namespace QindaQt::Network::SecretAgent
