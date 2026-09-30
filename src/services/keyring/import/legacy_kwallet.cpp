// SPDX-License-Identifier: GPL-3.0-or-later
#include "legacy_wire_p.h"
namespace qindaqt::keyring::importer {
namespace {
constexpr auto Interface="org.kde.KWallet";
constexpr auto Application="org.qindaqt.KeyringImport";
QString path(const Wire &wire) {return wire.binding().service.endsWith('5')?"/modules/kwalletd5":"/modules/kwalletd6";}
QStringList list(Wire &wire,const QString &method,const Append &append={},qsizetype maximum=1024) {
    auto reply=wire.call(path(wire),Interface,method,append,"as");auto iter=begin(reply.get());auto result=texts(iter,DBUS_TYPE_STRING,maximum);end(iter);result.sort();return result;
}
void arguments(DBusMessageIter &iter,int handle,const QString &folder,const QString &key) {
    appendInt(iter,handle);appendText(iter,folder);appendText(iter,key);appendText(iter,Application);
}
int open(Wire &wire,const QString &wallet) {
    auto reply=wire.call(path(wire),Interface,"openAsync",[&](auto &iter) {
        appendText(iter,wallet);const dbus_int64_t window=0;const dbus_bool_t session=1;
        if(!dbus_message_iter_append_basic(&iter,DBUS_TYPE_INT64,&window)) throw Failure{CollectionImportError::Unavailable};
        appendText(iter,Application);
        if(!dbus_message_iter_append_basic(&iter,DBUS_TYPE_BOOLEAN,&session)) throw Failure{CollectionImportError::Unavailable};
    },"i");
    auto iter=begin(reply.get());const int transaction=integer(iter);end(iter);
    if(transaction<0) throw Failure{CollectionImportError::Unavailable};
    reply.reset();
    auto completed=wire.waitSignal(path(wire),Interface,"walletAsyncOpened","ii",[transaction](DBusMessage *message) {
        auto event=begin(message);return integer(event)==transaction;
    });
    auto event=begin(completed.get());integer(event);const auto handle=integer(event);end(event);
    if(handle<0) throw Failure{CollectionImportError::Cancelled};
    return handle;
}
}
LegacySnapshot readKWallet(Wire &wire) {
    LegacySnapshot snapshot;snapshot.kind="kwallet";
    const auto wallets=list(wire,"wallets",{},64);
    for(const auto &wallet:wallets) {
        // openAsync may create a missing wallet. Re-enumerate immediately and
        // never intentionally request a name absent from the owning provider.
        if(!list(wire,"wallets",{},64).contains(wallet)) throw Failure{CollectionImportError::Conflict};
        const auto handle=open(wire,wallet);
        struct Close {Wire &wire;int handle;~Close(){try {wire.call(path(wire),Interface,"close",[&](auto &iter) {
            appendInt(iter,handle);const dbus_bool_t force=0;dbus_message_iter_append_basic(&iter,DBUS_TYPE_BOOLEAN,&force);appendText(iter,Application);
        },"i");} catch(...) {}}} close{wire,handle};
        LegacyCollectionSnapshot collection;collection.sourceId=collection.label=wallet;
        const auto folderArgs=[&](auto &iter){appendInt(iter,handle);appendText(iter,Application);};
        const auto folders=list(wire,"folderList",folderArgs);
        std::size_t aggregate=4;
        for(const auto &folder:folders) {
            const auto entryArgs=[&](auto &iter){appendInt(iter,handle);appendText(iter,folder);appendText(iter,Application);};
            const auto entries=list(wire,"entryList",entryArgs);
            for(const auto &key:entries) {
                if(collection.items.size()>=1024) throw Failure{CollectionImportError::Capacity};
                auto type=wire.call(path(wire),Interface,"entryType",[&](auto &iter){arguments(iter,handle,folder,key);},"i");
                auto iter=begin(type.get());LegacyItemSnapshot item;item.sourceId=key;item.folder=folder;item.entryType=integer(iter);end(iter);
                if(item.entryType<1 || item.entryType>3) throw Failure{CollectionImportError::InvalidInput};
                type.reset();auto reply=wire.call(path(wire),Interface,"readEntry",[&](auto &i){arguments(i,handle,folder,key);},"ay");
                iter=begin(reply.get());item.secret=bytes(iter);end(iter);item.metadata.label=key.toStdString();
                item.metadata.contentType="application/vnd.kde.kwallet-entry";
                aggregate+=256+static_cast<std::size_t>(folder.toUtf8().size())+2*static_cast<std::size_t>(key.toUtf8().size())+item.secret.size();
                if(aggregate>4*1024*1024) throw Failure{CollectionImportError::Capacity};
                collection.items.push_back(std::move(item));
            }
            if(list(wire,"entryList",entryArgs)!=entries) throw Failure{CollectionImportError::Conflict};
        }
        if(list(wire,"folderList",folderArgs)!=folders) throw Failure{CollectionImportError::Conflict};
        snapshot.collections.push_back(std::move(collection));
    }
    if(list(wire,"wallets",{},64)!=wallets) throw Failure{CollectionImportError::Conflict};
    return snapshot;
}
}
