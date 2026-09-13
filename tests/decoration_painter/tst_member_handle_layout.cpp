// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QImage>
#include <QPainter>
#include <QTest>

using namespace QindaQt::Decoration;

namespace {

DecorationChrome handleChrome(bool glyph)
{
    DecorationChrome chrome;
    chrome.surface = QColor(QStringLiteral("#20242a"));
    chrome.surfaceRaised = QColor(QStringLiteral("#2c313a"));
    chrome.border = QColor(QStringLiteral("#59636f"));
    chrome.text = QColor(QStringLiteral("#f4f7fb"));
    chrome.textMuted = QColor(QStringLiteral("#aeb8c4"));
    chrome.close = QColor(QStringLiteral("#e96c67"));
    chrome.minimize = QColor(QStringLiteral("#e6be57"));
    chrome.maximize = QColor(QStringLiteral("#62c36b"));
    chrome.buttonStyle = glyph ? QStringLiteral("glyph")
                               : QStringLiteral("symbols");
    chrome.buttonSide = glyph ? QStringLiteral("right")
                              : QStringLiteral("left");
    if (glyph) {
        chrome.titleBar = QColor(QStringLiteral("#2b6fd4"));
        chrome.titleBarInactive = QColor(QStringLiteral("#7e96b8"));
    }
    return chrome;
}

QImage paintHandleOnly(const DecorationChrome &chrome, const QSizeF &size,
                       bool maximized)
{
    QImage image(size.toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    DecorationFrameVisual frame;
    frame.size = size;
    frame.active = true;
    frame.maximized = maximized;
    frame.memberHandle = true;
    paintMemberHandle(painter, chrome, frame);
    painter.end();
    return image;
}

} // namespace

class MemberHandleLayoutTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void smallestSupportedWidthKeepsTargetsAndDragRegionDisjoint();
    void roomyWidthKeepsPreferredMetrics();
    void narrowerWidthDoesNotAdvertiseACompleteLayout();
    void gripPixelsStayOutOfControlTargets();
};

void MemberHandleLayoutTests::smallestSupportedWidthKeepsTargetsAndDragRegionDisjoint()
{
    QCOMPARE(DecorationMemberHandleMinimumWidth, 108.0);
    const QSizeF size(DecorationMemberHandleMinimumWidth, 80.0);
    const QRectF handle(QPointF(0.0, 0.0),
                        QSizeF(size.width(), DecorationMemberHandleHeight));

    for (const bool glyph : {false, true}) {
        const auto layout = layoutMemberHandle(handleChrome(glyph), size);
        QVERIFY(layout.supported);
        QCOMPARE(layout.buttons.size(), 4);
        QCOMPARE(layout.grip.center().x(), size.width() / 2.0);
        QVERIFY(layout.grip.width() >= DecorationMemberGripMinimumWidth);
        QVERIFY(layout.dragRegion.width() > 0.0);
        QVERIFY(layout.dragRegion.contains(layout.grip));
        QCOMPARE(layout.dragRegion.height(), DecorationMemberHandleHeight);

        for (qsizetype i = 0; i < layout.buttons.size(); ++i) {
            const QRectF target = layout.buttons.at(i).geometry;
            QVERIFY(handle.contains(target));
            QVERIFY(!target.intersects(layout.grip));
            QVERIFY(!target.intersects(layout.dragRegion));
            for (qsizetype j = i + 1; j < layout.buttons.size(); ++j) {
                QVERIFY(!target.intersects(layout.buttons.at(j).geometry));
            }
        }

        const auto more = layout.buttons.constLast();
        QCOMPARE(more.kind, DecorationButtonKind::More);
        if (glyph) {
            QCOMPARE(more.geometry.left(), DecorationMiniButtonInset);
            QCOMPARE(layout.buttons.constFirst().geometry.right(),
                     size.width() - DecorationMiniButtonInset
                         - 2.0 * (DecorationMiniButtonCell
                                  + DecorationMiniButtonSpacing));
        } else {
            QCOMPARE(layout.buttons.constFirst().geometry.left(),
                     DecorationMiniButtonInset);
            QCOMPARE(more.geometry.right(),
                     size.width() - DecorationMiniButtonInset);
        }
    }
}

void MemberHandleLayoutTests::roomyWidthKeepsPreferredMetrics()
{
    const QSizeF size(320.0, 200.0);
    for (const bool glyph : {false, true}) {
        const auto chrome = handleChrome(glyph);
        const auto layout = layoutMemberHandle(chrome, size);
        QVERIFY(layout.supported);
        QCOMPARE(layout.grip.width(), DecorationMemberGripMaximumWidth);
        QCOMPARE(layout.grip.center().x(), size.width() / 2.0);
        QCOMPARE(layout.buttons.size(), 4);
        QCOMPARE(layoutMemberHandleButtons(chrome, size).size(),
                 layout.buttons.size());
        for (qsizetype i = 0; i < layout.buttons.size(); ++i) {
            QCOMPARE(layoutMemberHandleButtons(chrome, size).at(i).geometry,
                     layout.buttons.at(i).geometry);
        }
    }
}

void MemberHandleLayoutTests::narrowerWidthDoesNotAdvertiseACompleteLayout()
{
    const auto below = layoutMemberHandle(
        handleChrome(false), QSizeF(DecorationMemberHandleMinimumWidth - 1.0, 80.0));
    QVERIFY(!below.supported);
    QVERIFY(below.grip.isEmpty());

    const auto shortFrame = layoutMemberHandle(
        handleChrome(false),
        QSizeF(DecorationMemberHandleMinimumWidth,
               DecorationMemberHandleHeight - 1.0));
    QVERIFY(!shortFrame.supported);
    QVERIFY(shortFrame.grip.isEmpty());
}

void MemberHandleLayoutTests::gripPixelsStayOutOfControlTargets()
{
    const QSizeF size(DecorationMemberHandleMinimumWidth, 80.0);
    for (const bool glyph : {false, true}) {
        const auto chrome = handleChrome(glyph);
        const auto layout = layoutMemberHandle(chrome, size);
        for (const bool maximized : {false, true}) {
            const QImage image = paintHandleOnly(chrome, size, maximized);
            const QColor clearBar = image.pixelColor(74, 7);
            QVERIFY(image.pixelColor(qRound(layout.grip.center().x()), 7)
                    != clearBar);
            for (const auto &button : layout.buttons) {
                const QRect control = button.geometry.toAlignedRect()
                                          .adjusted(2, 2, -2, -2);
                for (int x = control.left(); x <= control.right(); ++x) {
                    QCOMPARE(image.pixelColor(x, 7), clearBar);
                }
            }
        }
    }
}

QTEST_MAIN(MemberHandleLayoutTests)
#include "tst_member_handle_layout.moc"
