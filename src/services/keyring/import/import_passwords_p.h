// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/collection_import.h>
#include "../daemon/process_prompt_provider.h"
#include <QElapsedTimer>
namespace qindaqt::keyring::importer {
// Explicit transferred anonymous pipe/unix stream, or the existing owned native
// helper composition. Every password is a bounded owned page, never argv/text.
class Passwords final:public CollectionImportPasswords {
public:
    Passwords(int transferredFd,service::ProcessPromptProvider &,std::function<bool()> admitted);
    ~Passwords() override;
    ImportPassword take(const QString &,const QString &,bool existing) override;
private:
    bool read(unsigned char *,std::size_t);
    int fd_=-1;
    QElapsedTimer deadline_;
    service::ProcessPromptProvider &provider_;
    std::function<bool()> admitted_;
};
}
