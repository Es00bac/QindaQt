// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prompt_provider.h"
#include <QProcess>
#include <map>
namespace qindaqt::keyring::service {
class ProcessPromptProvider final : public PromptProvider {
public:
    explicit ProcessPromptProvider(QString executable, QObject *parent = nullptr);
    ~ProcessPromptProvider() override;
    quint64 begin(PromptRequest, PromptCompletion) override;
    void cancel(quint64) override;
private:
    struct Active { std::unique_ptr<QProcess> process; PromptCompletion done; SecureBuffer input; std::size_t used = 0; };
    void finish(quint64 id, bool cancelled);
    QString executable_;
    quint64 generation_ = 0;
    std::map<quint64,std::unique_ptr<Active>> active_;
};
}
