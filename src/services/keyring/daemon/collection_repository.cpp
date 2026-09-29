// SPDX-License-Identifier: GPL-3.0-or-later
#include "collection_repository.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

namespace qindaqt::keyring::service {
namespace {
[[noreturn]] void fail() { throw std::runtime_error("Collection unavailable"); }
quint64 now() { return static_cast<quint64>(QDateTime::currentSecsSinceEpoch()); }
}
bool validName(const QString &id) {
    static const QRegularExpression expression("^[A-Za-z0-9_.-]{1,60}$");
    return id.size() <= 60 && id != "." && id != ".." && expression.match(id).hasMatch();
}
Attributes attributes(const StringMap &wire) {
    if (wire.size() > 32) fail();
    Attributes result;
    for (auto i = wire.cbegin(); i != wire.cend(); ++i) {
        if (i.key().isEmpty() || i.key().size() > 1024 || i.value().size() > 1024 || i.key().toUtf8().size() > 1024 || i.value().toUtf8().size() > 1024) fail();
        result.emplace(i.key().toStdString(), i.value().toStdString());
    }
    return result;
}
StringMap attributes(const Attributes &stored) {
    StringMap result;
    for (const auto &[name, value] : stored) result.insert(QString::fromStdString(name), QString::fromStdString(value));
    return result;
}
SecureBuffer copySecret(const SecureBuffer &source) {
    SecureBuffer copy(source.size()); std::copy(source.bytes().begin(), source.bytes().end(), copy.bytes().begin()); return copy;
}
CollectionRepository::CollectionRepository(QString directory)
    : directory_(std::move(directory)), files_(directory_) {
    const auto diskNames = files_.collections();
    const auto catalog = files_.read("catalog.json", 131072);
    QJsonObject meta, aliases;
    QStringList names = diskNames;
    if (!catalog.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(catalog);
        if (!doc.isObject() || doc.object().value("version").toInt() != 1
            || !doc.object().value("collections").isObject() || !doc.object().value("aliases").isObject()) fail();
        meta = doc.object().value("collections").toObject();
        aliases = doc.object().value("aliases").toObject();
        if (meta.size() > 64 || aliases.size() > 64) fail();
        names = meta.keys();
    }
    for (const auto &id : names) {
        if (!validName(id) || id == "session" || !diskNames.contains(id)) fail();
        auto c = std::make_unique<Collection>();
        c->id = c->label = id;
        if (!catalog.isEmpty()) {
            if (!meta.value(id).isObject()) fail();
            const auto info = meta.value(id).toObject();
            if (!info.value("label").isString() || !info.value("created").isString()
                || !info.value("modified").isString()) fail();
            c->label = info.value("label").toString();
            if (c->label.size() > 1024 || c->label.toUtf8().size() > 1024) fail();
            bool validCreated = false, validModified = false;
            c->created = info.value("created").toString().toULongLong(&validCreated);
            c->modified = info.value("modified").toString().toULongLong(&validModified);
            if (!validCreated || !validModified) fail();
        }
        c->storage = std::make_unique<CollectionStore>(directory_.toStdString(), id.toStdString());
        if (c->storage->load() != StoreError::None) fail();
        collections_.emplace(id, std::move(c));
    }
    for (auto i = aliases.begin(); i != aliases.end(); ++i) {
        if (!validName(i.key()) || !i.value().isString() || !find(i.value().toString())
            || i.value().toString() == "session") fail();
        aliases_[i.key()] = i.value().toString();
    }
    if (catalog.isEmpty() && find("login")) aliases_["default"] = "login";
    // AGENT-CONTRACT: the durable catalog is the visibility commit. A crash
    // before create publication or after delete publication leaves an orphan
    // encrypted file; it is never advertised or unlocked (ADR-0296).
    if (!catalog.isEmpty()) for (const auto &id : diskNames)
        if (!names.contains(id)) files_.remove(id + ".qkr");
    auto session = std::make_unique<Collection>();
    session->id = session->label = "session"; session->created = session->modified = now();
    collections_.emplace("session", std::move(session));
    if (catalog.isEmpty()) persistCatalog();
}
QStringList CollectionRepository::names() const {
    QStringList names; for (const auto &[id, c] : collections_) { (void)c; names.append(id); } return names;
}
Collection *CollectionRepository::find(const QString &id) {
    const auto it = collections_.find(id); return it == collections_.end() ? nullptr : it->second.get();
}
bool CollectionRepository::locked(const QString &id) const {
    const auto i = collections_.find(id);
    return i == collections_.end() || (i->second->storage ? i->second->storage->locked() : i->second->volatileLocked);
}
SearchResult CollectionRepository::search(const QString &id, const Attributes &query) const {
    const auto i = collections_.find(id);
    if (i == collections_.end()) return {};
    if (i->second->storage) return i->second->storage->search(query);
    SearchResult result{{}, true};
    for (const auto &[itemId, item] : i->second->volatileItems) {
        bool matches = true;
        for (const auto &[name, value] : query) {
            const auto found = item.attributes.find(name);
            if (found == item.attributes.end() || found->second != value) { matches = false; break; }
        }
        if (matches) result.ids.push_back(itemId);
    }
    return result;
}
const Item *CollectionRepository::item(const QString &id, const QString &itemId) const {
    const auto i = collections_.find(id);
    if (i == collections_.end()) return nullptr;
    if (i->second->storage) return i->second->storage->item(itemId.toStdString());
    const auto it = i->second->volatileItems.find(itemId.toStdString());
    return it == i->second->volatileItems.end() ? nullptr : &it->second;
}
bool CollectionRepository::kdfAllowed() {
    if (kdfClock_.isValid() && kdfClock_.elapsed() < 500) return false;
    kdfClock_.start(); return true;
}
QString CollectionRepository::create(QString label, QString aliasName, std::span<const unsigned char> password) {
    if ((!aliasName.isEmpty() && !validName(aliasName)) || label.size() > 1024 || label.toUtf8().size() > 1024
        || password.empty() || password.size() > 4096 || collections_.size() >= 65 || !kdfAllowed()) fail();
    if (!aliasName.isEmpty() && aliases_.contains(aliasName)) return aliases_[aliasName];
    const auto id = aliasName == "default" && !find("login") ? QString("login")
        : QString("c") + QUuid::createUuid().toString(QUuid::Id128);
    auto c = std::make_unique<Collection>();
    c->id = id; c->label = std::move(label); c->created = c->modified = now();
    c->storage = std::make_unique<CollectionStore>(directory_.toStdString(), id.toStdString());
    if (c->storage->create(password) != StoreError::None || c->storage->save() != StoreError::None) fail();
    collections_.emplace(id, std::move(c));
    if (!aliasName.isEmpty()) aliases_[aliasName] = id;
    persistCatalog(); return id;
}
bool CollectionRepository::unlock(const QString &id, std::span<const unsigned char> password) {
    auto c = find(id);
    if (c && !c->storage) { c->volatileLocked = false; return true; }
    return c && !password.empty() && password.size() <= 4096 && kdfAllowed()
        && (!c->storage || c->storage->unlock(password) == StoreError::None);
}
bool CollectionRepository::rekey(const QString &id, std::span<const unsigned char> password) {
    auto c = find(id);
    if (!c || !c->storage || password.empty() || password.size() > 4096 || !kdfAllowed()) return false;
    const auto result = c->storage->rekey(password);
    if (result == StoreError::None) return true;
    c->storage->load(); // Retire uncertain key/state and expose only locked metadata.
    return false;
}
void CollectionRepository::lock(const QString &id) {
    auto c = find(id); if (!c) fail();
    if (c->storage) c->storage->lock(); else { c->volatileItems.clear(); c->volatileLocked = true; }
}
bool CollectionRepository::put(const QString &id, Item item) {
    auto c = find(id); if (!c || locked(id)) return false;
    if (c->storage) {
        if (c->storage->put(std::move(item)) != StoreError::None) return false;
        if (c->storage->save() != StoreError::None) { c->storage->load(); return false; }
    } else {
        if ((!c->volatileItems.contains(item.id) && c->volatileItems.size() >= 1024)
            || item.secret.size() > 1024 * 1024 || item.metadata.label.size() > 1024
            || item.metadata.contentType.size() > 128 || item.attributes.size() > 32) return false;
        std::size_t total = item.secret.size() + item.metadata.label.size() + item.metadata.contentType.size();
        const auto account = [](const Item &value) {
            std::size_t bytes = value.secret.size() + value.metadata.label.size() + value.metadata.contentType.size() + 256;
            for (const auto &[key,v] : value.attributes) bytes += key.size() + v.size() + 16;
            return bytes;
        };
        for (const auto &[key,value] : item.attributes) {
            if (key.empty() || key.size() > 1024 || value.size() > 1024) return false;
        }
        total = account(item);
        for (const auto &[key,value] : c->volatileItems) if (key != item.id) total += account(value);
        if (total > 4 * 1024 * 1024) return false;
        auto itemId = item.id; c->volatileItems.insert_or_assign(std::move(itemId), std::move(item));
    }
    c->modified = now(); persistCatalog(); return true;
}
bool CollectionRepository::erase(const QString &id, const QString &itemId) {
    auto c = find(id); if (!c || locked(id)) return false;
    if (c->storage) {
        if (c->storage->erase(itemId.toStdString()) != StoreError::None) return false;
        if (c->storage->save() != StoreError::None) { c->storage->load(); return false; }
    } else if (!c->volatileItems.erase(itemId.toStdString())) return false;
    c->modified = now(); persistCatalog(); return true;
}
void CollectionRepository::remove(const QString &id) {
    if (!find(id) || id == "session" || locked(id)) fail();
    collections_.erase(id);
    for (auto i = aliases_.begin(); i != aliases_.end();) {
        if (i.value() == id) i = aliases_.erase(i); else ++i;
    }
    persistCatalog();
    files_.remove(id + ".qkr");
}
QString CollectionRepository::alias(const QString &name) const { return aliases_.value(name); }
void CollectionRepository::setAlias(const QString &name, const QString &id) {
    if ((!aliases_.contains(name) && aliases_.size() >= 64) || !validName(name) || (!id.isEmpty() && (!find(id) || id == "session"))) fail();
    if (id.isEmpty()) aliases_.remove(name); else aliases_[name] = id;
    persistCatalog();
}
void CollectionRepository::setLabel(const QString &id, const QString &label) {
    auto c = find(id); if (!c || locked(id) || label.size() > 1024 || label.toUtf8().size() > 1024) fail();
    c->label = label; c->modified = now(); persistCatalog();
}
void CollectionRepository::persistCatalog() {
    QJsonObject meta, aliases;
    for (const auto &[id, c] : collections_) if (id != "session")
        meta[id] = QJsonObject{{"label", c->label}, {"created", QString::number(c->created)}, {"modified", QString::number(c->modified)}};
    for (auto i = aliases_.begin(); i != aliases_.end(); ++i) aliases[i.key()] = i.value();
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"collections", meta}, {"aliases", aliases}}).toJson(QJsonDocument::Compact);
    if (bytes.size() > 131072) fail();
    files_.replace("catalog.json", bytes);
}
}
