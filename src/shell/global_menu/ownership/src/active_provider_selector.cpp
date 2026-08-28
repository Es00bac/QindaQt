// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/global_menu/ownership/active_provider_selector.h>

namespace QindaQt::Shell::GlobalMenu::Ownership
{

void ActiveProviderSelector::adopt(const AuthenticatedProvider &proof)
{
    // AGENT-GUARD: `proof` is opaque and authenticator-issued, so the fields
    // read here are exactly the verified facts — no caller-supplied window or
    // process id can enter the selection.
    const QUuid ownerWindowId = proof.window().windowId;
    const bool sameWindowLineage = m_current && m_current->window.windowId == ownerWindowId;
    const QUuid epoch = sameWindowLineage ? m_current->epoch : QUuid::createUuid();
    const quint64 revision = sameWindowLineage ? m_current->revision + 1 : 1;

    m_current = SelectedProvider{.window = proof.window(),
                                  .providerUniqueName = proof.providerUniqueName(),
                                  .epoch = epoch,
                                  .revision = revision,
                                  .focusGeneration = proof.focusGeneration()};
}

void ActiveProviderSelector::clear()
{
    m_current.reset();
}

void ActiveProviderSelector::applyFocusGeneration(quint64 generation)
{
    if (m_current && m_current->focusGeneration != generation) {
        m_current.reset();
    }
}

std::optional<SelectedProvider> ActiveProviderSelector::current() const
{
    return m_current;
}

} // namespace QindaQt::Shell::GlobalMenu::Ownership
