// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtCore/QObject>

#include <functional>

namespace QindaQt::Network::SecretAgent {

// The controller owns request policy and deadlines. Implementations own only
// presentation objects on the constructing GUI thread. They must clear every
// editor before invoking the one-shot completion and before destroying a
// canceled prompt.
class PromptPort : public QObject {
  Q_OBJECT

public:
  using Completion = std::function<void(PromptResult)>;

  using QObject::QObject;
  ~PromptPort() override = default;

  [[nodiscard]] virtual bool showPrompt(const PromptRequest &request,
                                        Completion completion) = 0;
  virtual void cancelPrompt(quint64 requestId) = 0;
};

} // namespace QindaQt::Network::SecretAgent
