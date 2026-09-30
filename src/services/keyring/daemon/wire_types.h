// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring_protocol/wire_types.h>
namespace qindaqt::keyring::service {
using protocol::Paths;
using protocol::StringMap;
using protocol::WireSecret;
using protocol::SecretMap;
using protocol::registerWireTypes;
using protocol::wipe;
using protocol::argument;
}
