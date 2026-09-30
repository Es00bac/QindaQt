// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <QTimer>
#include <QUuid>
#include <QCoreApplication>
namespace qindaqt::keyring::service {
QString SecretService::addPrompt(Prompt prompt) {
    int owned = 0;
    for (const auto &[path,p] : prompts_) { (void)path; if (p.owner == prompt.owner) ++owned; }
    if (prompts_.size() >= 32 || owned >= 4) throw std::runtime_error("Prompt limit");
    const auto path = QString(Root) + "/prompt/p" + QUuid::createUuid().toString(QUuid::Id128);
    prompts_.emplace(path,std::move(prompt));
    QTimer::singleShot(60000,this,[this,path] { finishPrompt(path,true); });
    return path;
}
void SecretService::finishPrompt(const QString &path, bool dismissed) {
    const auto found = prompts_.find(path); if (found == prompts_.end()) return;
    auto p = std::move(found->second); prompts_.erase(found);
    promptsProvider_.cancel(p.ticket);
    if (dismissed && p.relockOnCancel) {
        repository_.lock(p.collections.first());notifyCollectionState(p.collections.first());
    }
    QVariant result = p.action == "create"
        ? QVariant::fromValue(p.completed.isEmpty() ? objectPath() : p.completed.first())
        : QVariant::fromValue(p.completed);
    if (p.action == "reveal") {
        WireSecret wire{objectPath(),{},{},"application/octet-stream"};
        try {
            if (!dismissed) {
                // AGENT-GUARD: publication checks independently admitted native
                // Unlocked, even when collection lock-on-screen policy is false.
                if(!nativeDisclosureAllowed()) throw std::runtime_error("Unavailable");
                const auto id = p.collections.first();
                const auto item = repository_.item(id,itemForPath(p.objects.first().path(),id));
                if (!item || repository_.locked(id)) throw std::runtime_error("Unavailable");
                wire = session(p.secretSession,p.owner).crypto.encode(objectPath(p.secretSession),
                    item->secret,QString::fromStdString(item->metadata.contentType));
            }
        } catch (const std::exception &) { dismissed = true;wipe(wire.value); }
        result = QVariant::fromValue(wire);
    }
    auto signal = QDBusMessage::createTargetedSignal(p.owner,path,"org.freedesktop.Secret.Prompt","Completed");
    signal.setArguments({dismissed,QVariant::fromValue(QDBusVariant(result))});
    bus_.send(signal);
}
void SecretService::startPrompt(const QString &path) {
    const auto initial = prompts_.find(path); if (initial == prompts_.end()) return;
    const auto &initialPrompt = initial->second;
    const auto requestedId = initialPrompt.action == "create" ? QString() : initialPrompt.collections.value(initialPrompt.next);
    const auto c = repository_.find(requestedId);
    PromptRequest request{initialPrompt.action,requestedId,(initialPrompt.action == "create" || initialPrompt.action == "confirm-delete") ? initialPrompt.label : c ? c->label : requestedId,initialPrompt.owner,{}};
    // Rate-limit admission globally in the repository; paced prompts avoid
    // bursts and keep private password bytes absent during the wait.
    QTimer::singleShot(550,this,[this,path,request] {
        const auto pending = prompts_.find(path); if (pending == prompts_.end()) return;
        if(pending->second.action=="reveal" && !nativeDisclosureAllowed()) {finishPrompt(path,true);return;}
        pending->second.ticket = promptsProvider_.begin(request,[this,path](SecureBuffer password,bool cancelled) {
            const auto found = prompts_.find(path); if (found == prompts_.end()) return;
            auto &p = found->second; p.ticket = 0;
            const std::size_t maximum = p.action == "change-password" ? 8200 : 4096;
            if (cancelled || password.size() == 0 || password.size() > maximum) { finishPrompt(path,true); return; }
            try {
                if (p.action == "confirm-delete") {
                    const auto bytes = password.bytes();
                    const bool approved = bytes.size() == 4 && bytes[0]=='Q' && bytes[1]=='K' && bytes[2]=='O' && bytes[3]=='K';
                    password.clear();
                    const auto id = p.collections.first(), itemId = itemForPath(p.objects.first().path(),id);
                    const bool saved = approved && !repository_.locked(id) && repository_.erase(id,itemId);
                    if (saved) {
                        p.completed = p.objects;
                        signal(collectionPath(id),CollectionInterface,"ItemDeleted",{variantPath(p.objects.first().path())});
                    }
                    finishPrompt(path,!saved);return;
                }
                if (p.action == "change-password") { changePromptPassword(path,std::move(password));return; }
                if (p.action == "reveal" && (!nativeDisclosureAllowed() || !sessions_.contains(p.secretSession)
                    || sessions_.at(p.secretSession)->owner != p.owner)) { finishPrompt(path,true);return; }
                if (p.action == "create") {
                    const auto id = repository_.create(p.label,p.alias,password.bytes());
                    p.completed.append(objectPath(collectionPath(id)));
                    notifyCollectionState(id); // Newly created collections obey resident lock policy before disclosure.
                    signal(Root,ServiceInterface,"CollectionCreated",{variantPath(collectionPath(id))});
                    changed(Root,ServiceInterface,properties(Root,ServiceInterface));
                    finishPrompt(path,false);
                } else {
                    const auto id = p.collections.value(p.next);
                    if (!repository_.unlock(id,password.bytes())) { finishPrompt(path,true); return; }
                    notifyCollectionState(id);
                    for (const auto &object : p.objects)
                        if (collectionForPath(object.path()) == id) p.completed.append(object);
                    ++p.next;
                    if (p.next >= p.collections.size()) finishPrompt(path,false); else startPrompt(path);
                }
            } catch (const PersistenceError &) {
                finishPrompt(path,true); QCoreApplication::exit(1);
            } catch (const std::exception &) { finishPrompt(path,true); }
        });
    });
}
bool SecretService::promptMethod(const QDBusMessage &m) {
    const auto found = prompts_.find(m.path());
    if (found == prompts_.end() || found->second.owner != m.service()) {
        error(m,"org.freedesktop.DBus.Error.AccessDenied"); return true;
    }
    if (m.member() == "Dismiss" && m.signature().isEmpty()) {
        reply(m); finishPrompt(m.path(),true); return true;
    }
    if (m.member() == "Prompt" && m.signature() == "s" && m.arguments()[0].toString().size() <= 1024) {
        if (found->second.running) { error(m,"org.freedesktop.DBus.Error.Failed"); return true; }
        found->second.running = true; reply(m); startPrompt(m.path()); return true;
    }
    error(m,"org.freedesktop.DBus.Error.UnknownMethod"); return true;
}
}
