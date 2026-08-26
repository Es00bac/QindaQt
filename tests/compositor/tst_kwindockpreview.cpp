// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwindockpreview.h"

#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;
namespace HybridInput = QindaQt::HybridInput;

namespace {

struct OverlayState final
{
    QRectF geometry;
    QColor accent;
    int shown = 0;
    int hidden = 0;
};

class FakeOverlay final : public DockPreviewOverlay
{
public:
    explicit FakeOverlay(OverlayState &state) : m_state(state) {}
    void showPreview(const QRectF &geometry, const QColor &accent) override
    {
        m_state.geometry = geometry;
        m_state.accent = accent;
        ++m_state.shown;
    }
    void hidePreview() noexcept override { ++m_state.hidden; }

private:
    OverlayState &m_state;
};

class FakeFactory final : public DockPreviewOverlayFactory
{
public:
    explicit FakeFactory(OverlayState &state) : m_state(state) {}
    std::unique_ptr<DockPreviewOverlay> create() override
    {
        return std::make_unique<FakeOverlay>(m_state);
    }

private:
    OverlayState &m_state;
};

HybridInput::InteractionIntent previewIntent(HybridInput::DockZone zone,
                                             HybridInput::IntentPhase phase)
{
    return {
        .kind = HybridInput::InteractionKind::MemberDock,
        .phase = phase,
        .source = {HybridInput::HitKind::MemberTitle, {}, QStringLiteral("source"), {}},
        .target = {{}, QStringLiteral("target"), zone},
        .position = {},
        .delta = {},
    };
}

} // namespace

class KWinDockPreviewTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void computesDirectionalAndTabGeometry();
    void followsPreviewLifecycle();
    void rejectsMalformedTargets();
};

void KWinDockPreviewTest::computesDirectionalAndTabGeometry()
{
    const QRectF target(100.0, 50.0, 800.0, 600.0);
    QCOMPARE(*KWinDockPreview::previewGeometry(target, HybridInput::DockZone::Left),
             QRectF(100.0, 50.0, 400.0, 600.0));
    QCOMPARE(*KWinDockPreview::previewGeometry(target, HybridInput::DockZone::Right),
             QRectF(500.0, 50.0, 400.0, 600.0));
    QCOMPARE(*KWinDockPreview::previewGeometry(target, HybridInput::DockZone::Top),
             QRectF(100.0, 50.0, 800.0, 300.0));
    QCOMPARE(*KWinDockPreview::previewGeometry(target, HybridInput::DockZone::Bottom),
             QRectF(100.0, 350.0, 800.0, 300.0));
    QCOMPARE(*KWinDockPreview::previewGeometry(target, HybridInput::DockZone::Tab),
             QRectF(112.0, 62.0, 776.0, 576.0));
}

void KWinDockPreviewTest::followsPreviewLifecycle()
{
    OverlayState state;
    FakeFactory factory(state);
    KWinDockPreview preview(
        [](const HybridInput::DockTarget &) {
            return std::optional<QRectF>(QRectF(0.0, 0.0, 1000.0, 700.0));
        },
        factory,
        QColor(QStringLiteral("#4daf98")));

    preview.handleIntent(previewIntent(HybridInput::DockZone::Left,
                                       HybridInput::IntentPhase::Update));
    QVERIFY(preview.visible());
    QCOMPARE(preview.geometry(), QRectF(0.0, 0.0, 500.0, 700.0));
    QCOMPARE(state.shown, 1);
    QCOMPARE(state.accent, QColor(QStringLiteral("#4daf98")));

    preview.handleIntent(previewIntent(HybridInput::DockZone::Left,
                                       HybridInput::IntentPhase::Commit));
    QVERIFY(!preview.visible());
    QCOMPARE(state.hidden, 1);
}

void KWinDockPreviewTest::rejectsMalformedTargets()
{
    QVERIFY(!KWinDockPreview::previewGeometry({}, HybridInput::DockZone::Left));
    QVERIFY(!KWinDockPreview::previewGeometry(QRectF(0, 0, 100, 100),
                                              HybridInput::DockZone::None));
}

QTEST_GUILESS_MAIN(KWinDockPreviewTest)
#include "tst_kwindockpreview.moc"
