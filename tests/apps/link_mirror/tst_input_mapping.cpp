// SPDX-License-Identifier: GPL-3.0-or-later
// Window-to-virtual-display input mapping for qindaqt-link-mirror.

#include "qindaqt/link_mirror/input_mapper.h"
#include "qindaqt/link_mirror/mirror_protocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace qindaqt::link_mirror;

namespace {

struct Sent {
    quint8 type = 0;
    QJsonObject body;
};

Sent only(const QList<QByteArray> &frames)
{
    if (frames.size() != 1) {
        return {};
    }
    return {static_cast<quint8>(frames.first().at(1)),
            QJsonDocument::fromJson(frames.first().mid(kHeaderSize)).object()};
}

ViewportMapper mapper(QSize display, QSizeF view)
{
    ViewportMapper result;
    result.setDisplaySize(display);
    result.setViewSize(view);
    return result;
}

} // namespace

class InputMappingTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void letterboxesAWideWindow()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(1000, 1000));
        QCOMPARE(m.contentRect(), QRectF(250, 0, 500, 1000));
        QVERIFY(!m.toDisplay(QPointF(100, 500)));
        QCOMPARE(m.toDisplay(QPointF(250, 0)).value(), QPointF(0, 0));
        QCOMPARE(m.toDisplay(QPointF(500, 500)).value(), QPointF(360, 720));
    }

    void pillarboxesATallWindow()
    {
        const ViewportMapper m = mapper(QSize(800, 400), QSizeF(400, 400));
        QCOMPARE(m.contentRect(), QRectF(0, 100, 400, 200));
        QVERIFY(!m.toDisplay(QPointF(200, 50)));
        QCOMPARE(m.toDisplay(QPointF(200, 200)).value(), QPointF(400, 200));
    }

    void clampsToTheLastPixel()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(360, 720));
        QVERIFY(!m.toDisplay(QPointF(360, 720)));
        QCOMPARE(m.toDisplayClamped(QPointF(5000, -40)).value(), QPointF(719, 0));
        ViewportMapper unknown;
        QVERIFY(!unknown.toDisplayClamped(QPointF(1, 1)));
    }

    void leftButtonDrivesOneFinger()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(360, 720));
        InputTranslator translator(m);
        Sent down = only(translator.mousePress(QPointF(180, 360), Qt::LeftButton));
        QCOMPARE(down.type, control::Touch);
        QCOMPARE(down.body.value(QStringLiteral("action")).toString(), QStringLiteral("down"));
        QCOMPARE(down.body.value(QStringLiteral("x")).toDouble(), 360.0);
        QCOMPARE(down.body.value(QStringLiteral("y")).toDouble(), 720.0);
        QVERIFY(translator.touching());
        QVERIFY(translator.mousePress(QPointF(10, 10), Qt::LeftButton).isEmpty()); // no second finger
        // A drag outside the window still moves and ends on the display edge.
        Sent move = only(translator.mouseMove(QPointF(-50, 360)));
        QCOMPARE(move.body.value(QStringLiteral("x")).toDouble(), 0.0);
        QVERIFY(translator.mouseMove(QPointF(-60, 360)).isEmpty()); // same clamped point
        Sent up = only(translator.mouseRelease(QPointF(-60, 360), Qt::LeftButton));
        QCOMPARE(up.body.value(QStringLiteral("action")).toString(), QStringLiteral("up"));
        QVERIFY(!translator.touching());
        QVERIFY(translator.mouseMove(QPointF(1, 1)).isEmpty()); // hover sends nothing
    }

    void pressOutsideTheFrameIsIgnored()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(1000, 1000));
        InputTranslator translator(m);
        QVERIFY(translator.mousePress(QPointF(10, 10), Qt::LeftButton).isEmpty());
        QVERIFY(translator.mouseRelease(QPointF(10, 10), Qt::LeftButton).isEmpty());
    }

    void secondaryButtonIsBack()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(360, 720));
        InputTranslator translator(m);
        const QList<QByteArray> frames = translator.mousePress(QPointF(5, 5), Qt::RightButton);
        QCOMPARE(frames.size(), 2);
        QCOMPARE(only({frames.at(0)}).body.value(QStringLiteral("keycode")).toInt(), android_key::Back);
        QCOMPARE(only({frames.at(1)}).body.value(QStringLiteral("action")).toString(), QStringLiteral("up"));
    }

    void wheelScrollsAtThePointer()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(360, 720));
        InputTranslator translator(m);
        Sent scroll = only(translator.wheel(QPointF(100, 100), QPoint(0, -240)));
        QCOMPARE(scroll.type, control::Scroll);
        QCOMPARE(scroll.body.value(QStringLiteral("v")).toDouble(), -2.0);
        QCOMPARE(scroll.body.value(QStringLiteral("x")).toDouble(), 200.0);
        QVERIFY(translator.wheel(QPointF(100, 100), QPoint()).isEmpty());
    }

    void typingSendsTextAndChordsSendKeys()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(360, 720));
        InputTranslator translator(m);
        Sent text = only(translator.keyPress(Qt::Key_A, Qt::ShiftModifier, QStringLiteral("A"), false));
        QCOMPARE(text.type, control::Text);
        QVERIFY(translator.keyRelease(Qt::Key_A, Qt::ShiftModifier, false).isEmpty());
        Sent chord = only(translator.keyPress(Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"), false));
        QCOMPARE(chord.type, control::Key);
        QCOMPARE(chord.body.value(QStringLiteral("keycode")).toInt(), 31);
        QCOMPARE(chord.body.value(QStringLiteral("meta")).toInt(), android_key::MetaCtrlOn);
        // Ctrl released first: the C key-up must still be sent.
        Sent up = only(translator.keyRelease(Qt::Key_C, Qt::NoModifier, false));
        QCOMPARE(up.body.value(QStringLiteral("action")).toString(), QStringLiteral("up"));
        Sent enter = only(translator.keyPress(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"), false));
        QCOMPARE(enter.body.value(QStringLiteral("keycode")).toInt(), 66);
        Sent repeat = only(translator.keyPress(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"), true));
        QCOMPARE(repeat.body.value(QStringLiteral("repeat")).toInt(), 1);
        QVERIFY(translator.keyRelease(Qt::Key_Return, Qt::NoModifier, true).isEmpty());
    }

    void releaseAllBalancesEveryDown()
    {
        const ViewportMapper m = mapper(QSize(720, 1440), QSizeF(360, 720));
        InputTranslator translator(m);
        QVERIFY(!translator.mousePress(QPointF(10, 10), Qt::LeftButton).isEmpty());
        QVERIFY(!translator.keyPress(Qt::Key_Left, Qt::NoModifier, QString(), false).isEmpty());
        const QList<QByteArray> frames = translator.releaseAll();
        QCOMPARE(frames.size(), 2);
        QCOMPARE(only({frames.at(0)}).body.value(QStringLiteral("action")).toString(), QStringLiteral("cancel"));
        QCOMPARE(only({frames.at(1)}).body.value(QStringLiteral("keycode")).toInt(), 21);
        QVERIFY(translator.releaseAll().isEmpty());
    }

    void mapsKeysAndModifiers()
    {
        QCOMPARE(androidKeycode(Qt::Key_Z), 54);
        QCOMPARE(androidKeycode(Qt::Key_0), 7);
        QCOMPARE(androidKeycode(Qt::Key_F12), 142);
        QCOMPARE(androidKeycode(Qt::Key_Backspace), 67);
        QCOMPARE(androidKeycode(Qt::Key_VolumeUp), 0);
        QCOMPARE(androidMetaState(Qt::ShiftModifier | Qt::AltModifier),
                 android_key::MetaShiftOn | android_key::MetaAltOn);
    }
};

QTEST_GUILESS_MAIN(InputMappingTest)
#include "tst_input_mapping.moc"
