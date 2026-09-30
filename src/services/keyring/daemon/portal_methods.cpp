// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <qindaqt/services/secret_portal/secret_policy.h>
#include <qindaqt/services/secret_portal/legacy_import.h>
#include <QDBusConnectionInterface>
#include <QScopeGuard>
#include <QUuid>
namespace qindaqt::keyring::service {
namespace portal=QindaQt::Services::SecretPortal;
bool SecretService::portalCaller(const QString &caller) const {
    if(!bus_.interface()) return false;
    const auto owner=bus_.interface()->serviceOwner(portal::BackendName);
    return owner.isValid() && owner.value()==caller;
}
SecureBuffer SecretService::portalSecret(const QString &app) {
    // AGENT-CONTRACT: fixed PK3 login identity, never the mutable default alias.
    // Only a domain-specific app record can be generated/read; there is no
    // caller-selected collection/item and no store/master-key export (ADR0310).
    if(!nativeDisclosureAllowed() || !portal::validApplicationId(app) || !repository_.find("login") || repository_.locked("login"))
        throw std::runtime_error("Portal secret unavailable");
    const auto itemId=portal::applicationItemId(app);
    if(!repository_.item("login",itemId)) {
        if(!repository_.put("login",portal::newApplicationSecret(app))) throw std::runtime_error("Portal secret persistence unavailable");
        signal(collectionPath("login"),CollectionInterface,"ItemCreated",{variantPath(itemPath("login",itemId))});
        changed(collectionPath("login"),CollectionInterface,properties(collectionPath("login"),CollectionInterface));
    }
    const auto *item=repository_.item("login",itemId);
    if(!item || !portal::supportedPortalSecret(*item,app) || !nativeDisclosureAllowed())
        throw std::runtime_error("Portal secret unavailable");
    return copySecret(item->secret);
}
bool SecretService::portalMethod(const QDBusMessage &message) {
    if(message.member()!="RequestPortalSecret") return false;
    const auto nonce=message.arguments().value(1).toString();
    const bool validNonce=QUuid::fromString(nonce).toString(QUuid::WithoutBraces)==nonce && !QUuid::fromString(nonce).isNull();
    if(message.signature()!="ss" || !portalCaller(message.service()) || !validNonce || !nativeDisclosureAllowed()) {
        error(message,"org.freedesktop.DBus.Error.AccessDenied");return true;
    }
    const auto publish=[&](const QByteArray &bytes,const QVariant &prompt) {
        auto signal=QDBusMessage::createTargetedSignal(message.service(),Root,NativeInterface,"PortalSecretResult");signal.setArguments({nonce,bytes,prompt});bus_.send(signal);reply(message);
    };
    const auto app=message.arguments()[0].toString();
    if(!portal::validApplicationId(app)) {error(message,"org.freedesktop.DBus.Error.InvalidArgs");return true;}
    if(!repository_.find("login") || repository_.locked("login")) {
        const bool create=!repository_.find("login");
        if(create && !repository_.alias("default").isEmpty()) {error(message,"org.freedesktop.DBus.Error.Failed");return true;}
        Prompt prompt;prompt.owner=message.service();prompt.action=create?"portal-create":"portal-unlock";
        prompt.label="Login";prompt.portalApplication=app;prompt.portalNonce=nonce;prompt.relockOnCancel=!create;
        if(!create) prompt.collections={"login"};
        publish(QByteArray{},variantPath(addPrompt(std::move(prompt))));return true;
    }
    auto pages=portalSecret(app);QByteArray bytes(reinterpret_cast<const char *>(pages.bytes().data()),static_cast<qsizetype>(pages.size()));
    auto scrub=qScopeGuard([&] {wipe(bytes);});
    if(!portalCaller(message.service()) || !nativeDisclosureAllowed()) throw std::runtime_error("Portal authority unavailable");
    publish(bytes,variantPath());return true;
}
}
