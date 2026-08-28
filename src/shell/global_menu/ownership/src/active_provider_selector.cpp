// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

void ActiveProviderSelector::adoptAuthenticated(const MenuProviderRegistration &registration,
                                                 const WindowIdentity &window)
{
    const bool sameWindowLineage = m_current && m_current->window.windowId == window.windowId;
    const QUuid epoch = sameWindowLineage ? m_current->epoch : QUuid::createUuid();
    const quint64 revision = sameWindowLineage ? m_current->revision + 1 : 1;

    m_current = SelectedProvider{.window = window,
                                  .providerUniqueName = registration.providerUniqueName,
                                  .epoch = epoch,
                                  .revision = revision};
}

void ActiveProviderSelector::clear()
{
    m_current.reset();
}

std::optional<SelectedProvider> ActiveProviderSelector::current() const
{
    return m_current;
}

} // namespace QindaQt::Shell::GlobalMenu::Ownership
