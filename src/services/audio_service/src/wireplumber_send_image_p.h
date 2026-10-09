// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

struct pw_impl_module;

namespace QindaQt::Audio
{
// Release-owned, compile-pinned package/image admission. Default builds never
// enable borrowed-Core sends. Only the exact qualified Portage revision may
// supply these inputs; a runtime version or caller environment cannot opt in.
class ConsoleSendImage final
{
public:
    [[nodiscard]] static bool enabled() noexcept;
    [[nodiscard]] bool admitted() const;
    [[nodiscard]] bool loaded(pw_impl_module *module) const;
};
} // namespace QindaQt::Audio
