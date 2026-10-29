// SPDX-License-Identifier: GPL-3.0-or-later
#include "wireplumber_audio_buffer_p.h"

#include <QtTest>

#include <limits>

using namespace QindaQt::Audio;

class AudioBufferTests final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void captureHonorsOffsetWrapAndCapacity()
    {
        std::array<float, 8> samples{0, 1, 2, 3, 4, 5, 6, 7};
        spa_chunk chunk{};
        chunk.offset = 6 * sizeof(float);
        chunk.size = 4 * sizeof(float);
        chunk.stride = AudioBuffer::kStereoStride;
        spa_data data{};
        data.data = samples.data();
        data.maxsize = sizeof(samples);
        data.chunk = &chunk;
        spa_buffer buffer{};
        buffer.n_datas = 1;
        buffer.datas = &data;
        auto view = AudioBuffer::capture(&buffer);
        QCOMPARE(view[0].size(), size_t(2));
        QCOMPARE(view[0][0], 6.0F);
        QCOMPARE(view[1].size(), size_t(2));
        QCOMPARE(view[1][1], 1.0F);
        chunk.offset += 2 * data.maxsize;
        chunk.size = std::numeric_limits<uint32_t>::max();
        view = AudioBuffer::capture(&buffer);
        QCOMPARE(view[0].size() + view[1].size(), samples.size());
        QCOMPARE(view[0][0], 6.0F);
        QCOMPARE(view[1].back(), 5.0F);
        chunk.size = 3 * sizeof(float);
        view = AudioBuffer::capture(&buffer);
        QCOMPARE(view[0].size() + view[1].size(), size_t(2)); // whole stereo frames
        data.maxsize = sizeof(samples) - 1; // a padded, non-wrapping plane
        chunk.offset = 0;
        chunk.size = data.maxsize;
        view = AudioBuffer::capture(&buffer);
        QCOMPARE(view[0].size(), size_t(6));
        QVERIFY(view[1].empty());
    }

    void malformedPlanesAreEmpty()
    {
        QVERIFY(AudioBuffer::capture(nullptr)[0].empty());
        QVERIFY(AudioBuffer::playback(nullptr, 1).empty());
        spa_buffer buffer{};
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        buffer.n_datas = 1;
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        spa_data data{};
        buffer.datas = &data;
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        std::array<float, 8> samples{};
        data.data = samples.data();
        data.maxsize = sizeof(samples);
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        spa_chunk chunk{};
        data.chunk = &chunk;
        chunk.size = sizeof(samples);
        chunk.offset = 1;
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        chunk.offset = 0;
        chunk.flags = SPA_CHUNK_FLAG_CORRUPTED;
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        chunk.flags = 0;
        chunk.stride = -1;
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        data.maxsize = 0;
        QVERIFY(AudioBuffer::capture(&buffer)[0].empty());
        QVERIFY(AudioBuffer::playback(&buffer, 1).empty());
        QCOMPARE(chunk.size, uint32_t(0));
    }

    void playbackBoundsTheFullWidthRequest()
    {
        std::array<float, 8> samples{};
        spa_chunk chunk{};
        spa_data data{};
        data.data = samples.data();
        data.maxsize = sizeof(samples);
        data.chunk = &chunk;
        spa_buffer buffer{};
        buffer.n_datas = 1;
        buffer.datas = &data;
        auto view = AudioBuffer::playback(&buffer, uint64_t(1) << 32);
        QCOMPARE(view.size(), samples.size());
        QCOMPARE(chunk.size, uint32_t(sizeof(samples)));
        QCOMPARE(chunk.offset, uint32_t(0));
        QCOMPARE(chunk.stride, int32_t(AudioBuffer::kStereoStride));
        view = AudioBuffer::playback(&buffer, 1);
        QCOMPARE(view.size(), size_t(2));
        QCOMPARE(chunk.size, AudioBuffer::kStereoStride);
        QCOMPARE(AudioBuffer::playback(&buffer, 0).size(), samples.size());
    }
};

QTEST_GUILESS_MAIN(AudioBufferTests)
#include "tst_audio_buffer.moc"
