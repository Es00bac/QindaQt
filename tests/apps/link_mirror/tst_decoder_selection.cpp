// SPDX-License-Identifier: GPL-3.0-or-later
// VA-API-first decoder selection with software fallback, using fake decoders.

#include "qindaqt/link_mirror/decoder_selection.h"

#include <QTest>

using namespace qindaqt::link_mirror;

namespace {

struct FakeBehaviour {
    bool vaapiOpens = true;
    bool vaapiDecodes = true;
    bool softwareOpens = true;
    bool softwareDecodes = true;
    QList<DecoderPath> created;
};

class FakeDecoder final : public VideoDecoder {
public:
    FakeDecoder(DecoderPath path, FakeBehaviour &behaviour) : m_path(path), m_behaviour(behaviour) {}
    [[nodiscard]] DecoderPath path() const override { return m_path; }
    bool open(const VideoFormat &, QString *error) override
    {
        const bool ok = m_path == DecoderPath::Vaapi ? m_behaviour.vaapiOpens : m_behaviour.softwareOpens;
        if (!ok) {
            *error = QStringLiteral("fake open failure");
        }
        return ok;
    }
    DecodeResult decode(const VideoPacket &packet, QImage *image, QString *error) override
    {
        const bool ok = m_path == DecoderPath::Vaapi ? m_behaviour.vaapiDecodes : m_behaviour.softwareDecodes;
        if (!ok) {
            *error = QStringLiteral("fake decode failure");
            return DecodeResult::Error;
        }
        if (packet.config) {
            return DecodeResult::NoFrame;
        }
        *image = QImage(2, 2, QImage::Format_RGB32);
        return DecodeResult::Frame;
    }

private:
    DecoderPath m_path;
    FakeBehaviour &m_behaviour;
};

DecoderFactory factory(FakeBehaviour &behaviour)
{
    return [&behaviour](DecoderPath path) {
        behaviour.created.append(path);
        return std::make_unique<FakeDecoder>(path, behaviour);
    };
}

const VideoFormat kFormat{QStringLiteral("h264"), QSize(720, 1440)};

VideoPacket keyPacket()
{
    VideoPacket packet;
    packet.key = true;
    packet.data = QByteArray("frame");
    return packet;
}

} // namespace

class DecoderSelectionTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void autoPrefersVaapi()
    {
        FakeBehaviour behaviour;
        DecoderSelector selector(DecoderPreference::Auto, factory(behaviour));
        QVERIFY(selector.open(kFormat));
        QCOMPARE(selector.activePath(), DecoderPath::Vaapi);
        QImage image;
        QCOMPARE(selector.decode(keyPacket(), &image), DecodeResult::Frame);
        QVERIFY(!image.isNull());
        QVERIFY(!selector.takeKeyframeRequest());
    }

    void autoFallsBackWhenVaapiCannotOpen()
    {
        FakeBehaviour behaviour;
        behaviour.vaapiOpens = false;
        DecoderSelector selector(DecoderPreference::Auto, factory(behaviour));
        QVERIFY(selector.open(kFormat));
        QCOMPARE(selector.activePath(), DecoderPath::Software);
        QVERIFY(selector.lastError().contains(QStringLiteral("vaapi")));
        QCOMPARE(behaviour.created, (QList<DecoderPath>{DecoderPath::Vaapi, DecoderPath::Software}));
    }

    void explicitVaapiDoesNotFallBack()
    {
        FakeBehaviour behaviour;
        behaviour.vaapiOpens = false;
        DecoderSelector selector(DecoderPreference::Vaapi, factory(behaviour));
        QVERIFY(!selector.open(kFormat));
        QVERIFY(!selector.isOpen());
        QCOMPARE(behaviour.created, QList<DecoderPath>{DecoderPath::Vaapi});
    }

    void explicitSoftwareNeverTouchesVaapi()
    {
        FakeBehaviour behaviour;
        DecoderSelector selector(DecoderPreference::Software, factory(behaviour));
        QVERIFY(selector.open(kFormat));
        QCOMPARE(behaviour.created, QList<DecoderPath>{DecoderPath::Software});
    }

    void hardwareDecodeErrorSwitchesToSoftwareForTheSession()
    {
        FakeBehaviour behaviour;
        behaviour.vaapiDecodes = false;
        DecoderSelector selector(DecoderPreference::Auto, factory(behaviour));
        QVERIFY(selector.open(kFormat));
        QImage image;
        QCOMPARE(selector.decode(keyPacket(), &image), DecodeResult::Error);
        QCOMPARE(selector.activePath(), DecoderPath::Software);
        QVERIFY(selector.takeKeyframeRequest());
        QVERIFY(!selector.takeKeyframeRequest());
        QCOMPARE(selector.decode(keyPacket(), &image), DecodeResult::Frame);
        // A resize opens a new generation; hardware stays disabled.
        behaviour.created.clear();
        QVERIFY(selector.open(VideoFormat{QStringLiteral("h264"), QSize(640, 1280)}));
        QCOMPARE(behaviour.created, QList<DecoderPath>{DecoderPath::Software});
    }

    void softwareDecodeErrorKeepsTheDecoder()
    {
        FakeBehaviour behaviour;
        behaviour.softwareDecodes = false;
        DecoderSelector selector(DecoderPreference::Software, factory(behaviour));
        QVERIFY(selector.open(kFormat));
        QImage image;
        QCOMPARE(selector.decode(keyPacket(), &image), DecodeResult::Error);
        QVERIFY(selector.isOpen());
        QVERIFY(!selector.takeKeyframeRequest());
        QVERIFY(selector.lastError().startsWith(QStringLiteral("software decode")));
    }

    void reportsWhenNothingOpens()
    {
        FakeBehaviour behaviour;
        behaviour.vaapiOpens = false;
        behaviour.softwareOpens = false;
        DecoderSelector selector(DecoderPreference::Auto, factory(behaviour));
        QVERIFY(!selector.open(kFormat));
        QVERIFY(selector.lastError().contains(QStringLiteral("software: fake open failure")));
        QImage image;
        QCOMPARE(selector.decode(keyPacket(), &image), DecodeResult::Error);
    }

    void backlogDropsUntilTheNextKeyframe()
    {
        BacklogGate gate(8);
        VideoPacket delta;
        delta.data = QByteArray("p");
        QCOMPARE(gate.admit(delta, 7), BacklogGate::Verdict::Queue);
        QCOMPARE(gate.admit(delta, 8), BacklogGate::Verdict::DropAndRequestKeyframe);
        QVERIFY(gate.dropping());
        // Even once the queue drains, deltas wait for a keyframe; only one sync is requested.
        QCOMPARE(gate.admit(delta, 0), BacklogGate::Verdict::Drop);
        VideoPacket config;
        config.config = true;
        config.data = QByteArray("sps");
        QCOMPARE(gate.admit(config, 9), BacklogGate::Verdict::Queue);
        QCOMPARE(gate.admit(keyPacket(), 9), BacklogGate::Verdict::Queue);
        QVERIFY(!gate.dropping());
        QCOMPARE(gate.admit(delta, 3), BacklogGate::Verdict::Queue);
    }

    void backlogResetsForANewGeneration()
    {
        BacklogGate gate(2);
        VideoPacket delta;
        delta.data = QByteArray("p");
        QCOMPARE(gate.admit(delta, 2), BacklogGate::Verdict::DropAndRequestKeyframe);
        gate.reset();
        QCOMPARE(gate.admit(delta, 0), BacklogGate::Verdict::Queue);
    }

    void parsesPreferences()
    {
        QCOMPARE(parseDecoderPreference(QStringLiteral("auto")), DecoderPreference::Auto);
        QCOMPARE(parseDecoderPreference(QStringLiteral("vaapi")), DecoderPreference::Vaapi);
        QCOMPARE(parseDecoderPreference(QStringLiteral("software")), DecoderPreference::Software);
        QVERIFY(!parseDecoderPreference(QStringLiteral("cuda")));
        QCOMPARE(decoderPathName(DecoderPath::Vaapi), QStringLiteral("vaapi"));
    }
};

QTEST_GUILESS_MAIN(DecoderSelectionTest)
#include "tst_decoder_selection.moc"
