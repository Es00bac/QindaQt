// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "crypto_p.h"

namespace qindaqt::keyring::detail {
StoreError readFile(const std::string &directory, const std::string &name, Bytes &bytes);
StoreError replaceFile(const std::string &directory, const std::string &name,
                       std::span<const unsigned char> bytes,
                       const std::function<bool()> &barrier, bool mustBeAbsent);
}
