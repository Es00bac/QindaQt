// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/audio_protocol/audio_types.h>

// One console the way Audio1 publishes it: two physical buses (one with a
// known target, one without), two virtual buses, a virtual strip and two
// hardware strips (one bound to a capture device, one unbound).
inline QindaQt::Audio::Snapshot consoleFixture()
{
    using namespace QindaQt::Audio;
    Snapshot snapshot;
    snapshot.epoch = 7;
    snapshot.revision = 42;
    Device speakers;
    speakers.handle = {7, 101};
    speakers.kind = DeviceKind::Output;
    speakers.name = QStringLiteral("Speakers");
    speakers.nodeName = QStringLiteral("alsa_output.pci-0000_00_1f.3.analog-stereo");
    snapshot.outputs.append(speakers);
    Device headphones;
    headphones.handle = {7, 102};
    headphones.kind = DeviceKind::Output;
    headphones.name = QStringLiteral("Headphones");
    headphones.nodeName = QStringLiteral("alsa_output.usb-dac.analog-stereo");
    snapshot.outputs.append(headphones);
    Device mic;
    mic.handle = {7, 202};
    mic.kind = DeviceKind::Input;
    mic.name = QStringLiteral("USB Mic");
    mic.nodeName = QStringLiteral("alsa_input.usb-mic.mono-fallback");
    snapshot.inputs.append(mic);

    Bus a1;
    a1.id = QStringLiteral("bus.a1");
    a1.kind = BusKind::Physical;
    a1.index = 0;
    a1.label = QStringLiteral("Speakers");
    a1.targetEpoch = 7;
    a1.targetSerial = 101;
    a1.targetKnown = true;
    a1.gainDb = -3.0;
    Bus a2;
    a2.id = QStringLiteral("bus.a2");
    a2.kind = BusKind::Physical;
    a2.index = 1;
    Bus b1;
    b1.id = QStringLiteral("bus.b1");
    b1.kind = BusKind::Virtual;
    b1.index = 0;
    b1.label = QStringLiteral("Chat");
    Bus b2;
    b2.id = QStringLiteral("bus.b2");
    b2.kind = BusKind::Virtual;
    b2.index = 1;
    b2.label = QStringLiteral("b2");
    snapshot.console.buses = {a1, a2, b1, b2};

    Strip virtual1;
    virtual1.id = QStringLiteral("strip.virtual.1");
    virtual1.kind = StripKind::VirtualInput;
    virtual1.index = 0;
    virtual1.label = QStringLiteral("Music");
    Strip hardware1;
    hardware1.id = QStringLiteral("strip.hardware.1");
    hardware1.kind = StripKind::HardwareInput;
    hardware1.index = 0;
    hardware1.label = QStringLiteral("Mic");
    hardware1.sourceEpoch = 7;
    hardware1.sourceSerial = 202;
    hardware1.sourceKnown = true;
    hardware1.muted = true;
    hardware1.gainDb = 2.5;
    Strip hardware2;
    hardware2.id = QStringLiteral("strip.hardware.2");
    hardware2.kind = StripKind::HardwareInput;
    hardware2.index = 1;
    snapshot.console.strips = {virtual1, hardware1, hardware2};
    return snapshot;
}
