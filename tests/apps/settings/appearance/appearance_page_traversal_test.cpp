// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance_page_traversal_test.h"

#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QSet>
#include <QtTest>

namespace {

QQuickItem *findVisualItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = findVisualItem(child, name); match != nullptr) {
            return match;
        }
    }
    return nullptr;
}

QQuickItem *namedFocusControl(QQuickItem *focused)
{
    for (QQuickItem *cursor = focused; cursor != nullptr;
         cursor = cursor->parentItem()) {
        if (cursor->objectName().startsWith(QStringLiteral("appearance"))) {
            return cursor;
        }
    }
    return nullptr;
}

bool isFullyVisibleIn(QQuickItem *control, QQuickItem *viewport)
{
    const QPointF topLeft = control->mapToItem(viewport, QPointF{});
    constexpr qreal tolerance = 1.0;
    return topLeft.y() >= -tolerance
        && topLeft.y() + control->height() <= viewport->height() + tolerance;
}

QString joined(const QSet<QString> &names)
{
    QStringList ordered(names.values());
    ordered.sort();
    return ordered.join(QStringLiteral(", "));
}

} // namespace

bool verifyFullAppearanceTraversal(QQuickView &view, QQuickItem *root,
                                   QString *error)
{
    view.resize(420, 320);
    QCoreApplication::processEvents();

    QQuickItem *firstCard = nullptr;
    if (!QTest::qWaitFor(
            [&]() {
                firstCard = findVisualItem(
                    root, QStringLiteral("appearanceThemeCard_qinda-dark"));
                return firstCard != nullptr;
            },
            1'000)) {
        *error = QStringLiteral("first qinda-dark theme card was not created");
        return false;
    }
    if (!QTest::qWaitFor([&]() { return firstCard->hasActiveFocus(); }, 1'000)) {
        *error = QStringLiteral("first qinda-dark theme card did not receive initial focus");
        return false;
    }
    auto *viewport = findVisualItem(
        root, QStringLiteral("appearanceFormViewport"));
    if (viewport == nullptr) {
        *error = QStringLiteral("Appearance form viewport was not created");
        return false;
    }

    const QSet<QString> required{
        QStringLiteral("appearanceThemeCard_qinda-dark"),
        QStringLiteral("appearanceSchemeButton_system"),
        QStringLiteral("appearanceSchemeButton_light"),
        QStringLiteral("appearanceSchemeButton_dark"),
        QStringLiteral("appearanceFontFamilyField"),
        QStringLiteral("appearanceFontSizeSlider"),
        QStringLiteral("appearanceAntialiasingSwitch"),
        QStringLiteral("appearanceHintingButton_none"),
        QStringLiteral("appearanceHintingButton_slight"),
        QStringLiteral("appearanceHintingButton_medium"),
        QStringLiteral("appearanceHintingButton_full"),
        QStringLiteral("appearanceSubpixelButton_none"),
        QStringLiteral("appearanceSubpixelButton_rgb"),
        QStringLiteral("appearanceSubpixelButton_bgr"),
        QStringLiteral("appearanceSubpixelButton_vrgb"),
        QStringLiteral("appearanceSubpixelButton_vbgr"),
        QStringLiteral("appearanceWallpaperField"),
        QStringLiteral("appearanceWallpaperModeButton_scaled"),
        QStringLiteral("appearanceWallpaperModeButton_centered"),
        QStringLiteral("appearanceWallpaperModeButton_tiled"),
        QStringLiteral("appearanceUiScaleSlider"),
        QStringLiteral("appearanceRevertButton"),
        QStringLiteral("appearanceApplyButton"),
        QStringLiteral("appearanceCloseButton"),
    };
    const QSet<QString> actionNames{
        QStringLiteral("appearanceRevertButton"),
        QStringLiteral("appearanceApplyButton"),
        QStringLiteral("appearanceCloseButton"),
    };

    const auto traverse = [&](Qt::Key key, QSet<QString> *visited) {
        for (int step = 0; step < 64; ++step) {
            auto *control = namedFocusControl(view.activeFocusItem());
            if (control == nullptr) {
                *error = QStringLiteral(
                    "focus left the named Appearance route at step %1").arg(step);
                return false;
            }
            visited->insert(control->objectName());
            if (required.contains(control->objectName())
                && !actionNames.contains(control->objectName())
                && !QTest::qWaitFor(
                    [&]() { return isFullyVisibleIn(control, viewport); },
                    1'000)) {
                *error = QStringLiteral("focused control remained clipped: %1")
                             .arg(control->objectName());
                return false;
            }
            QTest::keyClick(&view, key);
            QCoreApplication::processEvents();
            if (view.activeFocusItem() == firstCard && step > 0) {
                visited->insert(firstCard->objectName());
                return true;
            }
        }
        *error = QStringLiteral("Appearance traversal did not wrap in 64 steps");
        return false;
    };

    QSet<QString> forward;
    if (!traverse(Qt::Key_Tab, &forward)) {
        return false;
    }
    for (const QString &name : required) {
        if (!forward.contains(name)) {
            *error = QStringLiteral("forward traversal missed %1; visited %2")
                         .arg(name, joined(forward));
            return false;
        }
    }

    firstCard->forceActiveFocus(Qt::TabFocusReason);
    if (!QTest::qWaitFor([&]() { return firstCard->hasActiveFocus(); }, 1'000)) {
        *error = QStringLiteral("could not restore first-card focus for reverse traversal");
        return false;
    }
    QSet<QString> reverse;
    if (!traverse(Qt::Key_Backtab, &reverse)) {
        return false;
    }
    for (const QString &name : required) {
        if (!reverse.contains(name)) {
            *error = QStringLiteral("reverse traversal missed %1; visited %2")
                         .arg(name, joined(reverse));
            return false;
        }
    }
    return true;
}
