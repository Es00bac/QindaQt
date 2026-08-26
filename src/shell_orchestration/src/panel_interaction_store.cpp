// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::ShellOrchestration {
namespace {

using Identity = ShellVisibility::PanelSurfaceIdentity;

struct InteractionCounts {
    qsizetype reveals = 0;
    qsizetype holds = 0;
};

struct LeaseRecord {
    Identity identity;
    PanelInteractionKind kind = PanelInteractionKind::Reveal;
};

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

bool validKind(PanelInteractionKind kind)
{
    switch (kind) {
    case PanelInteractionKind::Reveal:
    case PanelInteractionKind::VisibilityHold:
        return true;
    }
    return false;
}

} // namespace

class PanelInteractionStore::Private final {
public:
    QHash<QString, QHash<QString, InteractionCounts>> byOutput;
    QHash<quint64, LeaseRecord> tokens;
    quint64 nextToken = 1;
};

PanelInteractionLease::PanelInteractionLease(
    PanelInteractionStore *store, quint64 token)
    : m_store(store)
    , m_token(token)
{
}

PanelInteractionLease::PanelInteractionLease(
    PanelInteractionLease &&other) noexcept
    : m_store(std::exchange(other.m_store, nullptr))
    , m_token(std::exchange(other.m_token, 0))
{
}

PanelInteractionLease &PanelInteractionLease::operator=(
    PanelInteractionLease &&other) noexcept
{
    if (this != &other) {
        reset();
        m_store = std::exchange(other.m_store, nullptr);
        m_token = std::exchange(other.m_token, 0);
    }
    return *this;
}

PanelInteractionLease::~PanelInteractionLease()
{
    reset();
}

bool PanelInteractionLease::valid() const noexcept
{
    return m_store && m_token != 0 && m_store->containsToken(m_token);
}

void PanelInteractionLease::reset()
{
    if (m_store && m_token != 0) {
        m_store->release(m_token);
    }
    m_store.clear();
    m_token = 0;
}

PanelInteractionStore::PanelInteractionStore(QObject *parent)
    : QObject(parent)
    , m_private(new Private)
{
}

PanelInteractionStore::~PanelInteractionStore()
{
    delete m_private;
}

bool PanelInteractionStore::setIdentities(
    const QVector<Identity> &identities, QString *error)
{
    QHash<QString, QHash<QString, InteractionCounts>> staged;
    for (const auto &identity : identities) {
        if (identity.panelId.trimmed().isEmpty() ||
            identity.outputId.trimmed().isEmpty()) {
            setError(error, QStringLiteral("panel interaction identity is empty"));
            return false;
        }
        auto &panels = staged[identity.outputId];
        if (panels.contains(identity.panelId)) {
            setError(error,
                     QStringLiteral("duplicate interaction identity '%1' on '%2'")
                         .arg(identity.panelId, identity.outputId));
            return false;
        }
        panels.insert(identity.panelId, {});
    }

    const auto before = snapshot();
    QHash<quint64, LeaseRecord> retainedTokens;
    for (auto token = m_private->tokens.cbegin();
         token != m_private->tokens.cend(); ++token) {
        auto output = staged.find(token->identity.outputId);
        if (output == staged.end()) {
            continue;
        }
        auto panel = output->find(token->identity.panelId);
        if (panel == output->end()) {
            continue;
        }
        if (token->kind == PanelInteractionKind::Reveal) {
            ++panel->reveals;
        } else {
            ++panel->holds;
        }
        retainedTokens.insert(token.key(), token.value());
    }
    m_private->byOutput = std::move(staged);
    m_private->tokens = std::move(retainedTokens);
    if (error) {
        error->clear();
    }
    if (before != snapshot()) {
        Q_EMIT interactionsChanged();
    }
    return true;
}

std::optional<PanelInteractionLease> PanelInteractionStore::acquire(
    const Identity &identity, PanelInteractionKind kind, QString *error)
{
    if (!validKind(kind)) {
        setError(error, QStringLiteral("panel interaction kind is invalid"));
        return std::nullopt;
    }
    auto output = m_private->byOutput.find(identity.outputId);
    if (output == m_private->byOutput.end()) {
        setError(error, QStringLiteral("panel interaction output is unknown"));
        return std::nullopt;
    }
    auto panel = output->find(identity.panelId);
    if (panel == output->end()) {
        setError(error, QStringLiteral("panel interaction identity is unknown"));
        return std::nullopt;
    }
    if (m_private->nextToken == 0) {
        setError(error, QStringLiteral("panel interaction token space is exhausted"));
        return std::nullopt;
    }
    const qsizetype activeCount = kind == PanelInteractionKind::Reveal
        ? panel->reveals
        : panel->holds;
    if (activeCount == std::numeric_limits<qsizetype>::max()) {
        setError(error, QStringLiteral("panel interaction lease count is exhausted"));
        return std::nullopt;
    }

    const quint64 token = m_private->nextToken;
    m_private->nextToken = token == std::numeric_limits<quint64>::max()
        ? 0
        : token + 1;
    bool changed = false;
    if (kind == PanelInteractionKind::Reveal) {
        changed = panel->reveals == 0;
        ++panel->reveals;
    } else {
        changed = panel->holds == 0;
        ++panel->holds;
    }
    m_private->tokens.insert(token, {identity, kind});
    if (error) {
        error->clear();
    }
    if (changed) {
        Q_EMIT interactionsChanged();
    }
    return PanelInteractionLease(this, token);
}

QVector<ShellVisibility::PanelInteractionSnapshot>
PanelInteractionStore::snapshot() const
{
    QVector<ShellVisibility::PanelInteractionSnapshot> result;
    for (auto output = m_private->byOutput.cbegin();
         output != m_private->byOutput.cend(); ++output) {
        for (auto panel = output->cbegin(); panel != output->cend(); ++panel) {
            result.append({{panel.key(), output.key()},
                           panel->reveals > 0,
                           panel->holds > 0});
        }
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.identity.outputId != right.identity.outputId) {
            return left.identity.outputId < right.identity.outputId;
        }
        return left.identity.panelId < right.identity.panelId;
    });
    return result;
}

bool PanelInteractionStore::containsToken(quint64 token) const noexcept
{
    return m_private->tokens.contains(token);
}

void PanelInteractionStore::release(quint64 token)
{
    const auto record = m_private->tokens.take(token);
    if (record.identity.panelId.isEmpty()) {
        return;
    }
    auto output = m_private->byOutput.find(record.identity.outputId);
    if (output == m_private->byOutput.end()) {
        return;
    }
    auto panel = output->find(record.identity.panelId);
    if (panel == output->end()) {
        return;
    }

    bool changed = false;
    if (record.kind == PanelInteractionKind::Reveal && panel->reveals > 0) {
        --panel->reveals;
        changed = panel->reveals == 0;
    } else if (record.kind == PanelInteractionKind::VisibilityHold && panel->holds > 0) {
        --panel->holds;
        changed = panel->holds == 0;
    }
    if (changed) {
        Q_EMIT interactionsChanged();
    }
}

} // namespace QindaQt::ShellOrchestration
