// SPDX-License-Identifier: GPL-3.0-or-later

// Scripted StatusNotifierSourceInterface seam for the applet controller tests.
// Records every dispatch, counts every observation call (the read-denied gate
// must never touch the seam), and can emit changed() re-entrantly inside an
// intent call to exercise the controller's exactly-once dispatch guard.

#pragma once

#include <qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h>
#include <qindaqt/shell/status_notifier/icon/status_notifier_icon_renderer.h>

#include <QtCore/QHash>

namespace QindaQt::StatusNotifierApplet::Tests {

using namespace QindaQt::StatusNotifier;

class FakeStatusNotifierSource final : public StatusNotifierSourceInterface {
    Q_OBJECT

public:
    struct RecordedCall {
        QString kind;
        OwnerKey key;
        int x = 0;
        int y = 0;
    };

    TrayPresentation m_presentation;
    QList<ItemDescriptor> m_descriptors;
    QHash<QString, quint64> m_generations;
    bool m_acceptIntents = true;
    QString m_refusalReason = QStringLiteral("scripted-refusal");
    // A null scripted icon renders the deterministic placeholder, like the
    // production seam does for a missing/undecodable icon.
    QImage m_scriptedIcon;
    // When armed, the intent methods emit changed() synchronously INSIDE the
    // dispatch call before returning — the hostile reentrancy vector.
    bool m_emitChangedInsideDispatch = false;

    // Observation counters: mutable because the seam's read contract is const.
    mutable int m_presentationCalls = 0;
    mutable int m_descriptorCalls = 0;
    mutable int m_renderCalls = 0;
    QList<RecordedCall> m_calls;
    int m_acknowledgeCalls = 0;

    [[nodiscard]] TrayPresentation presentation() const override
    {
        ++m_presentationCalls;
        return m_presentation;
    }

    [[nodiscard]] QList<ItemDescriptor> itemDescriptors() const override
    {
        ++m_descriptorCalls;
        return m_descriptors;
    }

    [[nodiscard]] QImage renderIcon(const OwnerKey &, int size) const override
    {
        ++m_renderCalls;
        if (!m_scriptedIcon.isNull()) {
            return m_scriptedIcon.scaled(size, size, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
        }
        return StatusNotifierIconRenderer::fallbackIcon(size);
    }

    [[nodiscard]] quint64 currentGeneration(const QString &uniqueName) const override
    {
        return m_generations.value(uniqueName, 0);
    }

    QindaQt::StatusNotifier::RegistryOutcome activate(const OwnerKey &target,
                                                      int x,
                                                      int y) override
    {
        return record(QStringLiteral("activate"), target, x, y);
    }

    QindaQt::StatusNotifier::RegistryOutcome secondaryActivate(const OwnerKey &target,
                                                               int x,
                                                               int y) override
    {
        return record(QStringLiteral("secondaryActivate"), target, x, y);
    }

    QindaQt::StatusNotifier::RegistryOutcome contextMenu(const OwnerKey &target,
                                                         int x,
                                                         int y) override
    {
        return record(QStringLiteral("contextMenu"), target, x, y);
    }

    bool m_itemIsMenu = false;
    bool m_exportedMenu = false;
    QVariantMap m_menuState;
    quint64 m_lastRevision = 0;
    int m_lastMenuId = -1;
    QString m_scrollOrientation;
    mutable int m_menuReads = 0;

    bool itemIsMenu(const OwnerKey &) const override { ++m_menuReads; return m_itemIsMenu; }
    bool hasExportedMenu(const OwnerKey &) const override { ++m_menuReads; return m_exportedMenu; }
    QVariantMap menuState(const OwnerKey &) const override { ++m_menuReads; return m_menuState; }
    RegistryOutcome invokeMenu(const OwnerKey &target, quint64 revision, int id) override
    {
        m_lastRevision = revision;
        m_lastMenuId = id;
        return record(QStringLiteral("invokeMenu"), target, 0, 0);
    }
    RegistryOutcome aboutToShowMenu(const OwnerKey &target, quint64 revision, int id) override
    {
        m_lastRevision = revision;
        m_lastMenuId = id;
        return record(QStringLiteral("aboutToShowMenu"), target, 0, 0);
    }
    RegistryOutcome scroll(const OwnerKey &target, int delta, const QString &orientation) override
    {
        m_scrollOrientation = orientation;
        return record(QStringLiteral("scroll"), target, delta, 0);
    }

    // Mirrors the production registry→seam recovery: acknowledging a scripted
    // Degraded presentation clears the diagnostic, restores the non-degraded
    // phase matching the scripted rows, and notifies — so controller tests
    // observe the same synchronous reprojection the real adapter produces.
    void acknowledgeDegraded() override
    {
        ++m_acknowledgeCalls;
        if (m_presentation.state == PresentationState::Degraded) {
            m_presentation.state = m_presentation.items.isEmpty()
                ? PresentationState::Empty
                : PresentationState::Ready;
            m_presentation.diagnostic.clear();
            Q_EMIT changed();
        }
    }

    void emitChanged()
    {
        Q_EMIT changed();
    }

private:
    QindaQt::StatusNotifier::RegistryOutcome record(const QString &kind,
                                                    const OwnerKey &key,
                                                    int x,
                                                    int y)
    {
        m_calls.append({ kind, key, x, y });
        if (m_emitChangedInsideDispatch) {
            Q_EMIT changed();
        }
        if (!m_acceptIntents) {
            return { RegistryStatus::InvalidRequest, m_refusalReason };
        }
        return {};
    }
};

// A presentation item plus its matching descriptor and live generation, the
// way a consistent registry snapshot presents one tray item.
inline TrayItemPresentation makePresentationItem(const QString &identity,
                                                 const QString &uniqueName,
                                                 const QString &objectPath,
                                                 quint64 generation,
                                                 const QString &accessibleName,
                                                 const QString &statusText)
{
    TrayItemPresentation item;
    item.owner = OwnerKey { uniqueName, objectPath, generation };
    item.identity = identity;
    item.accessibleName = accessibleName;
    item.accessibleDescription = QStringLiteral("Description for %1").arg(identity);
    item.accessibleStatusText = statusText;
    item.keyboardActions = {
        KeyboardAction { RequestKind::Activate, QStringLiteral("Enter or Space") },
        KeyboardAction { RequestKind::ContextMenu, QStringLiteral("Shift+F10 or Menu key") },
        KeyboardAction { RequestKind::SecondaryActivate, QString() },
    };
    return item;
}

} // namespace QindaQt::StatusNotifierApplet::Tests
