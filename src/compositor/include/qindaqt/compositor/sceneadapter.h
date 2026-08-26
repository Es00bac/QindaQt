// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

#include <memory>

namespace QindaQt::Core {
class WindowContainer;
}

namespace QindaQt::Compositor {

class SceneTransaction
{
public:
    virtual ~SceneTransaction() = default;

    // AGENT-CONTRACT: A false result means the adapter restored all live KWin
    // window/scene state. The bridge will preserve the prior model and revision.
    [[nodiscard]] virtual bool commit(QString *error = nullptr) = 0;
};

class SceneAdapter
{
public:
    virtual ~SceneAdapter() = default;

    // AGENT-CONTRACT: The KWin-derived adapter resolves stable window IDs and
    // stages geometry/ownership changes on KWin's main thread. Returning null
    // must leave the live scene untouched.
    [[nodiscard]] virtual std::unique_ptr<SceneTransaction> prepareTransition(
        const Core::WindowContainer &before,
        const Core::WindowContainer &after,
        QString *error = nullptr) = 0;
};

} // namespace QindaQt::Compositor
