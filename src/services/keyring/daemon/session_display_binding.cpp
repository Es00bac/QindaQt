// SPDX-License-Identifier: GPL-3.0-or-later
#include "session_display_binding.h"
namespace qindaqt::keyring::service {
SessionDisplayBinding::SessionDisplayBinding(QDBusConnection bus,QString runtime,bool required,QObject *parent)
    :QObject(parent),required_(required),attachment_(std::move(bus),std::move(runtime),
        [this](const QString &owner){return !selectedSessionOwner_.isEmpty() && owner==selectedSessionOwner_;}) {
    connect(&attachment_,&QindaQt::Platform::Compositor::CompositorAttachment::attached,this,&SessionDisplayBinding::attached);
    connect(&attachment_,&QindaQt::Platform::Compositor::CompositorAttachment::revoked,this,&SessionDisplayBinding::revoked);
}
SessionDisplayBinding::~SessionDisplayBinding()=default;
bool SessionDisplayBinding::attach(const QString &owner,const QString &name) {
    // AGENT-CONTRACT: SecretService's same-UID/first-session-owner admission
    // precedes PromptProvider::bindSessionDisplay. Select only that approved
    // owner here; the public attachment proves ordinary identity/lifetime,
    // never executable attestation or the separate privileged locker trust.
    required_=true;selectedSessionOwner_=owner;
    if(attachment_.attach(owner,name)) return true;
    selectedSessionOwner_.clear();return false;
}
bool SessionDisplayBinding::live() {
    return attachment_.live() || (!required_ && selectedSessionOwner_.isEmpty());
}
int SessionDisplayBinding::openPromptConnection() {
    if(!required_ && selectedSessionOwner_.isEmpty()) return -2;
    return attachment_.openConnection();
}
QString SessionDisplayBinding::basename() const {
    const auto current=attachment_.identity();return current?current->socketBasename:QString();
}
QString SessionDisplayBinding::compositorOwner() const {
    const auto current=attachment_.identity();return current?current->compositorOwner:QString();
}
quint64 SessionDisplayBinding::compositorPid() const {
    const auto current=attachment_.identity();return current?current->compositorPid:0;
}
}
