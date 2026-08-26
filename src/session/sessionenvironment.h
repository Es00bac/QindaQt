// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "sessionoptions.h"

namespace QindaQt::Session {

class SessionEnvironment final
{
public:
    static void apply(const SessionOptions &options);
};

} // namespace QindaQt::Session
