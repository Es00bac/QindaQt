// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/connection_authority.h>
#include <qindaqt/services/network_secret_agent/prompt_port.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <functional>

class QTimer;

namespace QindaQt::Network::SecretAgent {

class SecretAgentController final : public QObject {
  Q_OBJECT

public:
  using Completion = std::function<void(SecretAgentResult, SecretReply)>;

  explicit SecretAgentController(PromptPort &prompt,
                                 ConnectionAuthority &authority,
                                 int promptTimeoutMilliseconds = 120'000,
                                 QObject *parent = nullptr);
  ~SecretAgentController() override;

  [[nodiscard]] bool requestSecrets(const GetSecretsRequest &request,
                                    Completion completion);
  void cancel(const QString &connectionPath, const QString &settingName);
  void cancelAll();
  [[nodiscard]] qsizetype pendingCount() const noexcept;
  [[nodiscard]] constexpr StorageDisposition storageDisposition() const {
    return StorageDisposition::NetworkManagerOwnsStorage;
  }

private:
  struct Pending final {
    PromptRequest prompt;
    Completion completion;
    QPointer<QTimer> timer;
  };

  void finish(PromptResult result);
  void finishCanceled(quint64 requestId);
  [[nodiscard]] quint64 nextRequestId();

  PromptPort &m_prompt;
  ConnectionAuthority &m_authority;
  QHash<quint64, Pending> m_pending;
  quint64 m_nextRequestId = 0;
  int m_promptTimeoutMilliseconds = 120'000;
};

} // namespace QindaQt::Network::SecretAgent
