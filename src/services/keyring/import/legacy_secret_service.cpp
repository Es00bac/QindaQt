// SPDX-License-Identifier: GPL-3.0-or-later
#include "legacy_wire_p.h"
#include <set>
#include <map>
namespace qindaqt::keyring::importer {
namespace {
constexpr auto Root="/org/freedesktop/secrets";
constexpr auto Service="org.freedesktop.Secret.Service";
constexpr auto Collection="org.freedesktop.Secret.Collection";
constexpr auto ItemInterface="org.freedesktop.Secret.Item";
Message property(Wire &wire,const QString &path,const QString &interface,const QString &name) {
    return wire.call(path,"org.freedesktop.DBus.Properties","Get",[&](auto &iter){appendText(iter,interface);appendText(iter,name);},"v");
}
QString stringProperty(Wire &wire,const QString &path,const QString &interface,const QString &name) {
    auto reply=property(wire,path,interface,name);auto iter=begin(reply.get());auto value=child(iter,DBUS_TYPE_VARIANT);auto result=text(value);end(value);return result;
}
std::uint64_t dateProperty(Wire &wire,const QString &path,const QString &interface,const QString &name) {
    auto reply=property(wire,path,interface,name);auto iter=begin(reply.get());auto value=child(iter,DBUS_TYPE_VARIANT);auto result=unsigned64(value);end(value);return result;
}
QStringList listProperty(Wire &wire,const QString &path,const QString &interface,const QString &name,qsizetype maximum) {
    auto reply=property(wire,path,interface,name);auto iter=begin(reply.get());auto value=child(iter,DBUS_TYPE_VARIANT);
    auto result=texts(value,DBUS_TYPE_OBJECT_PATH,maximum);end(value);result.sort();return result;
}
bool locked(Wire &wire,const QString &path,const QString &interface) {
    auto reply=property(wire,path,interface,"Locked");auto iter=begin(reply.get());auto value=child(iter,DBUS_TYPE_VARIANT);auto result=boolean(value);end(value);return result;
}
void unlock(Wire &wire,const QStringList &paths) {
    if(paths.isEmpty()) return;
    auto reply=wire.call(Root,Service,"Unlock",[&](auto &iter){appendPaths(iter,paths);},"aoo");
    auto iter=begin(reply.get());auto unlocked=texts(iter,DBUS_TYPE_OBJECT_PATH);const auto prompt=text(iter,DBUS_TYPE_OBJECT_PATH);end(iter);reply.reset();
    if(prompt!="/") {
        try {
            wire.call(prompt,"org.freedesktop.Secret.Prompt","Prompt",[](auto &i){appendText(i,QString{});});
            auto completed=wire.waitSignal(prompt,"org.freedesktop.Secret.Prompt","Completed","bv",[](DBusMessage *){return true;});
            auto event=begin(completed.get());if(boolean(event)) throw Failure{CollectionImportError::Cancelled};
            auto result=child(event,DBUS_TYPE_VARIANT);const auto more=texts(result,DBUS_TYPE_OBJECT_PATH);end(result);end(event);
            for(const auto &path:more) if(!unlocked.contains(path)) unlocked.append(path);
        } catch(...) {try {wire.call(prompt,"org.freedesktop.Secret.Prompt","Dismiss");} catch(...) {}throw;}
    }
    for(const auto &path:unlocked) if(!paths.contains(path)) throw Failure{CollectionImportError::InvalidInput};
    for(const auto &path:paths) if(!unlocked.contains(path)) throw Failure{CollectionImportError::Unavailable};
}
QString session(Wire &wire) {
    auto reply=wire.call(Root,Service,"OpenSession",[](auto &iter) {
        appendText(iter,"plain");DBusMessageIter variant;
        if(!dbus_message_iter_open_container(&iter,DBUS_TYPE_VARIANT,"s",&variant)) throw Failure{CollectionImportError::Unavailable};
        appendText(variant,QString{});
        if(!dbus_message_iter_close_container(&iter,&variant)) throw Failure{CollectionImportError::Unavailable};
    },"vo");
    auto iter=begin(reply.get());auto output=child(iter,DBUS_TYPE_VARIANT);
    if(!text(output).isEmpty()) throw Failure{CollectionImportError::InvalidInput};
    end(output);
    const auto path=text(iter,DBUS_TYPE_OBJECT_PATH);end(iter);
    if(path=="/") throw Failure{CollectionImportError::InvalidInput};
    return path;
}
void secrets(Wire &wire,const QString &sessionPath,LegacyCollectionSnapshot &collection) {
    if(collection.items.empty()) return;
    QStringList paths;std::map<QString,LegacyItemSnapshot *> expected;
    for(auto &item:collection.items) {paths.append(item.sourceId);expected.emplace(item.sourceId,&item);}
    auto reply=wire.call(Root,Service,"GetSecrets",[&](auto &iter){appendPaths(iter,paths);appendText(iter,sessionPath,DBUS_TYPE_OBJECT_PATH);},"a{o(oayays)}");
    auto iter=begin(reply.get());auto entries=child(iter,DBUS_TYPE_ARRAY);
    std::set<QString> returned;std::size_t total=0;
    while(dbus_message_iter_get_arg_type(&entries)!=DBUS_TYPE_INVALID) {
        auto entry=child(entries,DBUS_TYPE_DICT_ENTRY);const auto path=text(entry,DBUS_TYPE_OBJECT_PATH);
        if(!expected.contains(path) || !returned.insert(path).second) throw Failure{CollectionImportError::InvalidInput};
        auto secret=child(entry,DBUS_TYPE_STRUCT);if(text(secret,DBUS_TYPE_OBJECT_PATH)!=sessionPath) throw Failure{CollectionImportError::InvalidInput};
        const auto parameters=bytes(secret,0);(void)parameters;
        auto value=bytes(secret);total+=value.size();if(total>4*1024*1024) throw Failure{CollectionImportError::Capacity};
        auto *item=expected.at(path);item->secret=std::move(value);item->metadata.contentType=text(secret,DBUS_TYPE_STRING,128).toStdString();end(secret);end(entry);
    }
    if(returned.size()!=expected.size()) throw Failure{CollectionImportError::Unavailable};
    end(iter);
}
}
LegacySnapshot readSecretService(Wire &wire,const QStringList &aliases) {
    LegacySnapshot snapshot;snapshot.kind="secret-service";
    const auto collections=listProperty(wire,Root,Service,"Collections",64);const auto sessionPath=session(wire);
    struct Close {Wire &wire;QString path;~Close(){try {wire.call(path,"org.freedesktop.Secret.Session","Close");} catch(...) {}}} close{wire,sessionPath};
    for(const auto &path:collections) {
        if(locked(wire,path,Collection)) unlock(wire,{path});
        if(locked(wire,path,Collection)) throw Failure{CollectionImportError::Unavailable};
        LegacyCollectionSnapshot collection;collection.sourceId=path;collection.label=stringProperty(wire,path,Collection,"Label");
        collection.created=dateProperty(wire,path,Collection,"Created");collection.modified=dateProperty(wire,path,Collection,"Modified");
        const auto items=listProperty(wire,path,Collection,"Items",1024);
        QStringList lockedItems;for(const auto &item:items) if(locked(wire,item,ItemInterface)) lockedItems.append(item);
        unlock(wire,lockedItems);
        for(const auto &itemPath:items) {
            if(locked(wire,itemPath,ItemInterface)) throw Failure{CollectionImportError::Unavailable};
            LegacyItemSnapshot item;item.sourceId=itemPath;item.metadata.label=stringProperty(wire,itemPath,ItemInterface,"Label").toStdString();
            item.metadata.created=dateProperty(wire,itemPath,ItemInterface,"Created");item.metadata.modified=dateProperty(wire,itemPath,ItemInterface,"Modified");
            auto reply=property(wire,itemPath,ItemInterface,"Attributes");auto iter=begin(reply.get());auto value=child(iter,DBUS_TYPE_VARIANT);
            item.attributes=stringMap(value);end(value);collection.items.push_back(std::move(item));
        }
        secrets(wire,sessionPath,collection);
        if(listProperty(wire,path,Collection,"Items",1024)!=items || dateProperty(wire,path,Collection,"Modified")!=collection.modified
            || stringProperty(wire,path,Collection,"Label")!=collection.label || locked(wire,path,Collection))
            throw Failure{CollectionImportError::Conflict};
        for(const auto &item:collection.items) if(dateProperty(wire,item.sourceId,ItemInterface,"Modified")!=item.metadata.modified)
            throw Failure{CollectionImportError::Conflict};
        snapshot.collections.push_back(std::move(collection));
    }
    if(listProperty(wire,Root,Service,"Collections",64)!=collections) throw Failure{CollectionImportError::Conflict};
    if(aliases.size()>64) throw Failure{CollectionImportError::Capacity};
    for(const auto &alias:aliases) {
        auto reply=wire.call(Root,Service,"ReadAlias",[&](auto &iter){appendText(iter,alias);},"o");auto iter=begin(reply.get());const auto path=text(iter,DBUS_TYPE_OBJECT_PATH);
        if(path!="/") {if(!collections.contains(path)) throw Failure{CollectionImportError::InvalidInput};snapshot.aliases[alias]=path;}
    }
    return snapshot;
}
}
