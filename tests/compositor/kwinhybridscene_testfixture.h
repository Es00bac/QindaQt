// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "kwinhybridscene.h"

#include <QHash>
#include <QSet>

namespace QindaQt::Compositor::Test {

class FakeHybridScenePlatform final
    : public KWinIntegration::KWinHybridScenePlatform
{
public:
    struct Window final
    {
        HybridConstraints::WindowRestoreState state;
        QRectF currentFrame;
        HybridConstraints::MemberSizeConstraints constraints;
    };

    void addWindow(QString id, Window window, QString owner = {})
    {
        owners.insert(id, std::move(owner));
        windows.insert(std::move(id), std::move(window));
    }

    void removeWindow(const QString &id)
    {
        windows.remove(id);
        owners.remove(id);
    }

    void addFocusOnlyWindow(QString id, bool focused = false)
    {
        focusOnlyWindows.insert(id);
        if (focused) {
            clearManagedFocus();
            focusOnlyActiveWindow = std::move(id);
        }
    }

    [[nodiscard]] QStringList windowIds() const override
    {
        auto result = windows.keys();
        result.sort();
        return result;
    }

    [[nodiscard]] bool windowExists(const QString &id) const override
    {
        return windows.contains(id);
    }

    [[nodiscard]] QString owner(const QString &id) const override
    {
        return owners.value(id);
    }

    [[nodiscard]] std::optional<QRectF> currentFrame(
        const QString &id, QString *error) const override
    {
        if (!windows.contains(id)) {
            assignError(error, QStringLiteral("missing fake window '%1'").arg(id));
            return std::nullopt;
        }
        return windows.value(id).currentFrame;
    }

    [[nodiscard]] std::optional<QRectF> placementArea(
        const QString &id,
        const QString &outputId,
        QString *error) const override
    {
        if (!windows.contains(id)) {
            assignError(error, QStringLiteral("missing fake window '%1'").arg(id));
            return std::nullopt;
        }
        const auto match = outputAreas.constFind(outputId);
        if (match == outputAreas.cend() || !match->isValid()) {
            assignError(error, QStringLiteral("missing fake output '%1'").arg(outputId));
            return std::nullopt;
        }
        return *match;
    }

    [[nodiscard]] std::optional<HybridConstraints::WindowRestoreState> captureState(
        const QString &id, QString *error) const override
    {
        if (!windows.contains(id)) {
            assignError(error, QStringLiteral("missing fake window '%1'").arg(id));
            return std::nullopt;
        }
        return windows.value(id).state;
    }

    [[nodiscard]] std::optional<HybridConstraints::MemberSizeConstraints>
    sizeConstraints(const QString &id, QString *error) const override
    {
        if (!windows.contains(id)) {
            assignError(error, QStringLiteral("missing fake window '%1'").arg(id));
            return std::nullopt;
        }
        return windows.value(id).constraints;
    }

    [[nodiscard]] bool validateState(
        const QString &id,
        const HybridConstraints::WindowRestoreState &state,
        QString *error) const override
    {
        return windows.contains(id) && state.isValid(error);
    }

    [[nodiscard]] bool applyState(
        const QString &id,
        const HybridConstraints::WindowRestoreState &state,
        QString *error) override
    {
        if (failApplyOnce.remove(id)) {
            assignError(error, QStringLiteral("injected apply failure for '%1'").arg(id));
            return false;
        }
        auto match = windows.find(id);
        if (match == windows.end()) {
            assignError(error, QStringLiteral("missing fake window '%1'").arg(id));
            return false;
        }
        const bool focused = match->state.focused;
        match->state = state;
        match->state.focused = focused;
        match->currentFrame = state.geometry;
        appliedStates.append(id);
        return true;
    }

    [[nodiscard]] QString activeWindowId() const override
    {
        if (!focusOnlyActiveWindow.isEmpty()) {
            return focusOnlyActiveWindow;
        }
        for (auto iterator = windows.cbegin(); iterator != windows.cend(); ++iterator) {
            if (iterator->state.focused) {
                return iterator.key();
            }
        }
        return {};
    }

    [[nodiscard]] bool activateWindow(const QString &id, QString *error) override
    {
        if (!windows.contains(id)) {
            assignError(error, QStringLiteral("missing fake window '%1'").arg(id));
            return false;
        }
        focusOnlyActiveWindow.clear();
        for (auto iterator = windows.begin(); iterator != windows.end(); ++iterator) {
            iterator->state.focused = iterator.key() == id;
        }
        return true;
    }

    [[nodiscard]] bool restoreFocus(const QString &id, QString *error) override
    {
        if (id.isEmpty()) {
            clearManagedFocus();
            focusOnlyActiveWindow.clear();
            return true;
        }
        if (focusOnlyWindows.contains(id)) {
            clearManagedFocus();
            focusOnlyActiveWindow = id;
            return true;
        }
        if (!windows.contains(id)) {
            Q_UNUSED(error);
            clearManagedFocus();
            focusOnlyActiveWindow.clear();
            return true;
        }
        return activateWindow(id, error);
    }

    [[nodiscard]] bool finalizeOwners(
        const QHash<QString, QString> &expected,
        const QHash<QString, QString> &candidate,
        const QHash<QString, QRectF> &targetFrames,
        const QSet<QString> &allowedMissing,
        QString *error) override
    {
        ++finalizeCalls;
        lastExpectedOwners = expected;
        lastCandidateOwners = candidate;
        lastTargetFrames = targetFrames;
        lastAllowedMissing = allowedMissing;
        if (failFinalize) {
            assignError(error, QStringLiteral("injected owner finalization failure"));
            return false;
        }
        for (auto iterator = candidate.cbegin(); iterator != candidate.cend(); ++iterator) {
            if (!windows.contains(iterator.key())) {
                if (allowedMissing.contains(iterator.key()) && iterator.value().isEmpty()) {
                    continue;
                }
                assignError(error, QStringLiteral("unexpected missing fake window"));
                return false;
            }
            if (owners.value(iterator.key()) != expected.value(iterator.key())) {
                assignError(error, QStringLiteral("fake owner precondition mismatch"));
                return false;
            }
        }
        for (auto iterator = candidate.cbegin(); iterator != candidate.cend(); ++iterator) {
            if (windows.contains(iterator.key())) {
                owners.insert(iterator.key(), iterator.value());
            }
        }
        return true;
    }

    static void assignError(QString *error, const QString &message)
    {
        if (error) {
            *error = message;
        }
    }

    void clearManagedFocus()
    {
        for (auto iterator = windows.begin(); iterator != windows.end(); ++iterator) {
            iterator->state.focused = false;
        }
    }

    QHash<QString, Window> windows;
    QHash<QString, QString> owners;
    QHash<QString, QRectF> outputAreas;
    QSet<QString> focusOnlyWindows;
    QString focusOnlyActiveWindow;
    QStringList appliedStates;
    QHash<QString, QString> lastExpectedOwners;
    QHash<QString, QString> lastCandidateOwners;
    QHash<QString, QRectF> lastTargetFrames;
    QSet<QString> lastAllowedMissing;
    QSet<QString> failApplyOnce;
    int finalizeCalls = 0;
    bool failFinalize = false;
};

inline HybridConstraints::WindowRestoreState richState(
    const QRectF &geometry, QString output, bool focused = false)
{
    return {
        .geometry = geometry,
        .minimized = false,
        .maximizedAxes = {},
        .quickTileEdges = {},
        .fullscreen = false,
        .outputId = std::move(output),
        .desktopIds = {QStringLiteral("desktop-a")},
        .activityIds = {QStringLiteral("activity-a")},
        .keepAbove = false,
        .keepBelow = false,
        .focused = focused,
    };
}

inline FakeHybridScenePlatform::Window fakeWindow(
    HybridConstraints::WindowRestoreState state,
    const QRectF &currentFrame,
    QSize minimum = QSize(40, 30))
{
    return {std::move(state),
            currentFrame,
            {.minimumSize = minimum,
             .maximumSize = std::nullopt,
             .fixedSize = std::nullopt}};
}

} // namespace QindaQt::Compositor::Test
