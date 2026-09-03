// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/prompt_port.h>

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QVariantList>

class QQmlEngine;

namespace QindaQt::Network::SecretAgent {

class QmlPromptPresenter final : public PromptPort {
  Q_OBJECT

public:
  explicit QmlPromptPresenter(QQmlEngine &engine, QObject *parent = nullptr);
  ~QmlPromptPresenter() override;

  [[nodiscard]] bool showPrompt(const PromptRequest &request,
                                Completion completion) override;
  void cancelPrompt(quint64 requestId) override;

  Q_INVOKABLE void submit(qulonglong requestId, const QVariantList &editors,
                          bool remember);
  Q_INVOKABLE void cancelByUser(qulonglong requestId);

private:
  struct ActivePrompt final {
    QPointer<QObject> window;
    PromptRequest request;
    Completion completion;
  };

  void closeAndForget(quint64 requestId);

  QQmlEngine &m_engine;
  QHash<quint64, ActivePrompt> m_active;
};

} // namespace QindaQt::Network::SecretAgent
