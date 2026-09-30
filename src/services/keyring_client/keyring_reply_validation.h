// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring_protocol/wire_types.h>
namespace QindaQt::Services::KeyringClient {
bool validObjectPath(const QString &path, const QString &component);
QVariantMap validateMetadata(const QVariantMap &wire, bool collection);
QVariantList validateCollections(const QVariantMap &wire);
QVariantList validateItems(const qindaqt::keyring::protocol::MetadataRows &wire);
}
