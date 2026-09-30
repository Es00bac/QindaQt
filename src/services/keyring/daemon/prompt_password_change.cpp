// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <QTimer>
#include <algorithm>
namespace qindaqt::keyring::service {
void SecretService::changePromptPassword(const QString &path,SecureBuffer frame) {
    const auto found=prompts_.find(path);if(found==prompts_.end()) return;
    const auto bytes=frame.bytes();
    // AGENT-CONTRACT: helper change-password output is QKP1 + two big-endian
    // u16 lengths + old/new UTF-8 bytes. Bound before allocation or KDF work.
    if(bytes.size()<8 || bytes[0]!='Q' || bytes[1]!='K' || bytes[2]!='P' || bytes[3]!='1') {
        finishPrompt(path,true);return;
    }
    const std::size_t oldSize=(std::size_t(bytes[4])<<8U)|bytes[5];
    const std::size_t newSize=(std::size_t(bytes[6])<<8U)|bytes[7];
    if(oldSize==0 || newSize==0 || oldSize>4096 || newSize>4096 || bytes.size()!=8+oldSize+newSize) {
        finishPrompt(path,true);return;
    }
    SecureBuffer oldPassword(oldSize);
    auto newPassword=std::make_shared<SecureBuffer>(newSize);
    std::copy_n(bytes.begin()+8,oldSize,oldPassword.bytes().begin());
    std::copy_n(bytes.begin()+static_cast<std::ptrdiff_t>(8+oldSize),newSize,newPassword->bytes().begin());
    frame.clear();
    auto &prompt=found->second;const auto id=prompt.collections.first();
    const bool wasLocked=repository_.locked(id);
    if(!repository_.unlock(id,oldPassword.bytes())) {
        oldPassword.clear();notifyCollectionState(id);finishPrompt(path,true);return;
    }
    oldPassword.clear();prompt.relockOnCancel=wasLocked;
    QTimer::singleShot(550,this,[this,path,id,newPassword] {
        const auto pending=prompts_.find(path);
        if(pending==prompts_.end()) { newPassword->clear();return; }
        const bool saved=repository_.rekey(id,newPassword->bytes());
        newPassword->clear();notifyCollectionState(id);
        if(saved) {
            pending->second.relockOnCancel=false;
            pending->second.completed={objectPath(collectionPath(id))};
        }
        finishPrompt(path,!saved);
    });
}
}
