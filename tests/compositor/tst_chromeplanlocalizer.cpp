// SPDX-License-Identifier: GPL-3.0-or-later
// localizeChromeRenderPlan moves a container's global plan into the frame-local
// space KWin's scene item paints. Every painted rectangle has to move with it;
// one that does not is drawn as if the container sat at the screen origin.
#include "chromeplanlocalizer.h"

#include "qindaqt/hybrid_chrome/chromelayoutengine.h"

#include <QtTest>

#include <tuple>

using namespace QindaQt::HybridChrome;
using QindaQt::Compositor::KWinIntegration::localizeChromeRenderPlan;

class ChromePlanLocalizerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void splitDeckPiecesFollowTheContainerAwayFromTheScreenOrigin();
};

// Owner, 2026-09-28: "Decorations on containers in the BeOS style themes ...
// don't stay attached to their windows. They appear to look correct-ish when
// the window is placed in the upper left corner of the screen."
void ChromePlanLocalizerTests::splitDeckPiecesFollowTheContainerAwayFromTheScreenOrigin()
{
    ChromeLayoutRequest request;
    request.containerId = QStringLiteral("container-deck");
    request.outerRect = QRectF(640.0, 380.0, 900.0, 500.0);
    request.containerFocused = true;
    request.style = ChromeStyle::standard(ButtonSide::Left);
    request.style.titleLayout = ContainerTitleLayout::SplitDeck;
    request.containerTitle = QStringLiteral("Container");
    request.containerTitleIsGenerated = true;
    request.containerTitleWidth = 64.0;
    for (int index = 0; index < 3; ++index) {
        request.tabs.append({QStringLiteral("page-%1").arg(index),
                             QStringLiteral("Page %1").arg(index), index == 1});
    }
    const QRectF inner = request.outerRect.adjusted(1.0, 1.0, -1.0, -1.0);
    request.members = {{QStringLiteral("member-a"), QStringLiteral("Editor"),
                        QRectF(inner.left(), inner.top() + 28.0, inner.width(),
                               inner.height() - 28.0)}};
    QString error;
    const auto global = ChromeLayoutEngine::build(request, &error);
    QVERIFY2(global.has_value(), qPrintable(error));
    QVERIFY(global->titleTab.isValid());
    QVERIFY(global->deckPiece.isValid());
    QVERIFY(global->titleLabelRect.isValid());

    const QPointF origin = global->outerFrame.topLeft();
    const auto local = localizeChromeRenderPlan(*global, origin);
    QCOMPARE(local.outerFrame.topLeft(), QPointF{});
    for (const auto &[name, globalRect, localRect] :
         {std::tuple{"titleTab", global->titleTab, local.titleTab},
          std::tuple{"deckPiece", global->deckPiece, local.deckPiece},
          std::tuple{"titleLabelRect", global->titleLabelRect, local.titleLabelRect},
          std::tuple{"tabStrip", global->tabStrip, local.tabStrip}}) {
        QVERIFY2(localRect == globalRect.translated(-origin), name);
        // The real consequence: each piece lands inside the painted image.
        QVERIFY2(local.outerFrame.adjusted(-0.5, -0.5, 0.5, 0.5).contains(localRect), name);
    }
}

QTEST_MAIN(ChromePlanLocalizerTests)
#include "tst_chromeplanlocalizer.moc"
