// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_client/audio_transport.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Apps::SettingsAudio::TestSupport {

using namespace QindaQt::Audio;

// Injected fake transport: the Settings Audio route tests never construct a
// bus, contact host PipeWire/WirePlumber, or link the resident service.
class FakeAudioTransport final : public AudioTransport {
public:
  struct Fetch final {
    QString owner;
    quint64 requestId = 0;
  };
  struct Operation final {
    QString owner;
    quint64 requestId = 0;
    OperationRequest request;
  };

  void start() override { ++startCalls; }
  void stop() override { ++stopCalls; }
  void fetchSnapshot(const QString &owner, const quint64 requestId) override {
    fetches.push_back({owner, requestId});
  }
  void submitOperation(const QString &owner, const quint64 requestId,
                       const OperationRequest &request) override {
    operations.push_back({owner, requestId, request});
  }

  void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
  void invalidate(const QString &owner, const quint64 epoch,
                  const quint64 revision) {
    Q_EMIT invalidated(owner, epoch, revision);
  }
  void reply(const Fetch &fetch, const Snapshot &snapshot) {
    Q_EMIT snapshotReply(fetch.owner, fetch.requestId, true, snapshot, {});
  }
  void fail(const Fetch &fetch, const QString &reasonCode) {
    Q_EMIT snapshotReply(fetch.owner, fetch.requestId, false, {}, reasonCode);
  }
  void finish(const Operation &operation, const OperationResult &result) {
    Q_EMIT operationReply(operation.owner, operation.requestId, true, result,
                          {});
  }
  void fail(const Operation &operation, const QString &reasonCode) {
    Q_EMIT operationReply(operation.owner, operation.requestId, false, {},
                          reasonCode);
  }

  QList<Fetch> fetches;
  QList<Operation> operations;
  int startCalls = 0;
  int stopCalls = 0;
};

// Protocol-valid ready snapshot: two hardware outputs (serial 10 default
// stereo, 12 six-channel surround), one managed virtual output (14), one
// input (serial 20 default), a playback stream (30) targeting the default
// output, and a capture stream (40) targeting the input. Serials are unique
// and ascending per list, and every can-set flag satisfies the protocol's
// known/capability preconditions.
inline Snapshot readyAudioSnapshot(const quint64 epoch = 11,
                                   const quint64 revision = 2) {
  Snapshot snapshot;
  snapshot.epoch = epoch;
  snapshot.revision = revision;
  snapshot.availability = Availability::Ready;
  snapshot.capabilities = Capability::SetDefault | Capability::SetVolume
                          | Capability::SetMute | Capability::MoveStream
                          | Capability::SetChannelVolumes
                          | Capability::ManageVirtualDevices;
  snapshot.defaultOutput = {.epoch = epoch, .serial = 10};
  snapshot.defaultInput = {.epoch = epoch, .serial = 20};
  snapshot.outputs = {
      {.handle = {.epoch = epoch, .serial = 10},
       .kind = DeviceKind::Output,
       .name = QStringLiteral("speakers"),
       .description = QStringLiteral("Desk Speakers"),
       .volume = 0.5,
       .volumeKnown = true,
       .muted = false,
       .muteKnown = true,
       .isDefault = true,
       .canSetVolume = true,
       .canSetMute = true,
       .channelVolumes = {0.5, 0.5},
       .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
       .virtualDevice = false},
      {.handle = {.epoch = epoch, .serial = 12},
       .kind = DeviceKind::Output,
       .name = QStringLiteral("surround"),
       .description = QStringLiteral("Surround Headphones"),
       .volume = 0.25,
       .volumeKnown = true,
       .muted = false,
       .muteKnown = true,
       .isDefault = false,
       .canSetVolume = true,
       .canSetMute = true,
       .channelVolumes = {0.25, 0.25, 0.25, 0.25, 0.25, 0.25},
       .channelMap = {QStringLiteral("FL"), QStringLiteral("FR"),
                      QStringLiteral("FC"), QStringLiteral("LFE"),
                      QStringLiteral("SL"), QStringLiteral("SR")},
       .virtualDevice = false},
      {.handle = {.epoch = epoch, .serial = 14},
       .kind = DeviceKind::Output,
       .name = QStringLiteral("qindaqt.virtual.game-bus"),
       .description = QStringLiteral("Game Bus"),
       .volume = 1.0,
       .volumeKnown = true,
       .muted = false,
       .muteKnown = true,
       .isDefault = false,
       .canSetVolume = true,
       .canSetMute = true,
       .channelVolumes = {1.0, 1.0},
       .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
       .virtualDevice = true},
  };
  snapshot.inputs = {
      {.handle = {.epoch = epoch, .serial = 20},
       .kind = DeviceKind::Input,
       .name = QStringLiteral("microphone"),
       .description = QStringLiteral("Desk Microphone"),
       .volume = 0.75,
       .volumeKnown = true,
       .muted = false,
       .muteKnown = true,
       .isDefault = true,
       .canSetVolume = true,
       .canSetMute = true,
       .channelVolumes = {0.75, 0.75},
       .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")},
       .virtualDevice = false},
  };
  snapshot.streams = {
      {.handle = {.epoch = epoch, .serial = 30},
       .direction = StreamDirection::Playback,
       .applicationName = QStringLiteral("Player"),
       .mediaName = QStringLiteral("Music"),
       .target = {.epoch = epoch, .serial = 10},
       .targetKnown = true,
       .volume = 0.75,
       .volumeKnown = true,
       .muted = false,
       .muteKnown = true,
       .canSetVolume = true,
       .canSetMute = true,
       .canMove = true,
       .channelVolumes = {0.75, 0.75},
       .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")}},
      {.handle = {.epoch = epoch, .serial = 40},
       .direction = StreamDirection::Capture,
       .applicationName = QStringLiteral("Talk"),
       .mediaName = QStringLiteral("Call"),
       .target = {.epoch = epoch, .serial = 20},
       .targetKnown = true,
       .volume = 0.4,
       .volumeKnown = true,
       .muted = true,
       .muteKnown = true,
       .canSetVolume = true,
       .canSetMute = true,
       .canMove = true,
       .channelVolumes = {0.4, 0.4},
       .channelMap = {QStringLiteral("FL"), QStringLiteral("FR")}},
  };
  return snapshot;
}

inline OperationResult audioResult(const OperationKind kind,
                                   const OperationStatus status,
                                   const quint64 epoch, const quint64 revision,
                                   QString reason = QStringLiteral("ok")) {
  return {.kind = kind,
          .status = status,
          .initiatingEpoch = epoch,
          .initiatingRevision = revision,
          .observedEpoch = epoch,
          .observedRevision = revision,
          .reasonCode = std::move(reason),
          .diagnostic = {},
          .wireValid = true};
}

} // namespace QindaQt::Apps::SettingsAudio::TestSupport
