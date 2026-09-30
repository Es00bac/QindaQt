// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "prompt_provider.h"
#include <QProcess>
#include <map>
namespace qindaqt::keyring::service {
class SessionDisplayBinding;
class ProcessPromptProvider final : public PromptProvider {
public:
    explicit ProcessPromptProvider(QString executable, QObject *parent = nullptr,
                                   SessionDisplayBinding *display = nullptr);
    ~ProcessPromptProvider() override;
    quint64 begin(PromptRequest, PromptCompletion) override;
    void cancel(quint64) override;
    bool bindSessionDisplay(const QString &,const QString &) override;
private:
    struct Active { ~Active(); std::unique_ptr<QProcess> process; PromptCompletion done; SecureBuffer input; QByteArray metadata; std::size_t used = 0, maximum = 4096; int displayFd = -1; };
    void finish(quint64 id, bool cancelled);
    // Borrowed GUI-thread collaborator outlives this provider; revocation cancels approval.
    SessionDisplayBinding *display_ = nullptr;
    QString executable_;
    quint64 generation_ = 0;
    std::map<quint64,std::unique_ptr<Active>> active_;
};
}
