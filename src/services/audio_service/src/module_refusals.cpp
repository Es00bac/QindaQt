// SPDX-License-Identifier: GPL-3.0-or-later

#include "module_refusals_p.h"

namespace QindaQt::Audio
{

bool ModuleRefusals::refused(const std::string &key, const QByteArray &arguments) const
{
    const auto it = m_refused.find(key);
    return it != m_refused.end() && it->second == arguments;
}

void ModuleRefusals::remember(const std::string &key, const QByteArray &arguments)
{
    m_refused.insert_or_assign(key, arguments);
}

void ModuleRefusals::retainDeclared(const std::unordered_map<std::string, QByteArray> &declared)
{
    std::erase_if(m_refused, [&declared](const auto &entry) {
        const auto wanted = declared.find(entry.first);
        return wanted == declared.end() || wanted->second != entry.second;
    });
}

void ModuleRefusals::clear() noexcept
{
    m_refused.clear();
}

} // namespace QindaQt::Audio
