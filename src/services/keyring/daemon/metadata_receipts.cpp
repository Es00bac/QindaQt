// SPDX-License-Identifier: GPL-3.0-or-later
#include "secret_service.h"
#include <QUuid>
namespace qindaqt::keyring::service {
QVariantMap SecretService::collectionMetadata() {
    QVariantMap result;
    for(const auto &id:repository_.names()) {
        const auto c=repository_.find(id);
        result[id]=QVariantMap{{"Path",variantPath(collectionPath(id))},{"Label",c->label},
            {"Locked",repository_.locked(id)},{"IndexAuthenticated",repository_.search(id,{}).authenticated}};
    }
    return result;
}
protocol::MetadataRows SecretService::itemMetadata(const QString &path) {
        const auto id=collectionForPath(path);
        if (id.isEmpty()) throw std::runtime_error("Invalid collection");
        const auto found=repository_.search(id,{});
        protocol::MetadataRows rows;
        for (const auto &value:found.ids) {
            const auto itemId=QString::fromStdString(value);
            QVariantMap row{{"Path",variantPath(itemPath(id,itemId))},
                {"Locked",repository_.locked(id)},{"IndexAuthenticated",found.authenticated}};
            const auto item=repository_.item(id,itemId);
            if (item) {
                row.insert("Label",QString::fromStdString(item->metadata.label));
                row.insert("Created",QVariant::fromValue(static_cast<quint64>(item->metadata.created)));
                row.insert("Modified",QVariant::fromValue(static_cast<quint64>(item->metadata.modified)));
                if (!item->metadata.creator.empty()) row.insert("CreatedBy",QString::fromStdString(item->metadata.creator));
            }
            rows.append(row);
        }
    return rows;
}
bool SecretService::metadataMethod(const QDBusMessage &message) {
    if(message.member()!="RequestMetadata") return false;
    const auto nonce=message.arguments().value(0).toString(),kind=message.arguments().value(1).toString();
    const auto parsed=QUuid::fromString(nonce);
    if(message.signature()!="sso" || parsed.isNull() || parsed.toString(QUuid::WithoutBraces)!=nonce || (kind!="collections" && kind!="items")) {
        error(message,"org.freedesktop.DBus.Error.InvalidArgs");return true;
    }
    const auto object=argument<QDBusObjectPath>(message.arguments()[2]).path();
    if(kind=="collections" && object!="/") {error(message,"org.freedesktop.DBus.Error.InvalidArgs");return true;}
    const auto rows=kind=="collections"?QVariant(collectionMetadata()):QVariant::fromValue(itemMetadata(object));
    // AGENT-CONTRACT: native UI metadata authority is the actual targeted signal
    // sender plus fresh request nonce/kind. RPC success only acknowledges transport.
    auto receipt=QDBusMessage::createTargetedSignal(message.service(),Root,NativeInterface,"MetadataReceipt");
    receipt.setArguments({nonce,kind,QVariant::fromValue(QDBusVariant(rows))});bus_.send(receipt);reply(message);return true;
}
}
