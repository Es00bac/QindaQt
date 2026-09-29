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
    const QVariant result = p.action == "create"
        ? QVariant::fromValue(p.completed.isEmpty() ? objectPath() : p.completed.first())
        : QVariant::fromValue(p.completed);
    auto signal = QDBusMessage::createTargetedSignal(p.owner,path,"org.freedesktop.Secret.Prompt","Completed");
    signal.setArguments({dismissed,QVariant::fromValue(QDBusVariant(result))});
    bus_.send(signal);
}
void SecretService::startPrompt(const QString &path) {
    const auto initial = prompts_.find(path); if (initial == prompts_.end()) return;
    const auto &initialPrompt = initial->second;
    const auto requestedId = initialPrompt.action == "create" ? QString() : initialPrompt.collections.value(initialPrompt.next);
    const auto c = repository_.find(requestedId);
    PromptRequest request{initialPrompt.action,requestedId,initialPrompt.action == "create" ? initialPrompt.label : c ? c->label : requestedId,initialPrompt.owner,{}};
    // Rate-limit admission globally in the repository; paced prompts avoid
    // bursts and keep private password bytes absent during the wait.
    QTimer::singleShot(550,this,[this,path,request] {
        const auto pending = prompts_.find(path); if (pending == prompts_.end()) return;
        pending->second.ticket = promptsProvider_.begin(request,[this,path](SecureBuffer password,bool cancelled) {
            const auto found = prompts_.find(path); if (found == prompts_.end()) return;
            auto &p = found->second; p.ticket = 0;
            if (cancelled || password.size() == 0 || password.size() > 4096) { finishPrompt(path,true); return; }
            try {
                if (p.action == "create") {
                    const auto id = repository_.create(p.label,p.alias,password.bytes());
                    p.completed.append(objectPath(collectionPath(id)));
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
