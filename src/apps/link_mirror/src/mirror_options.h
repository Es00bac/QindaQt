// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/link_mirror/decoder_selection.h"

#include <QList>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>

#include <optional>

// Command line of qindaqt-link-mirror (see docs/wiki/reference/qindalink-mirror-v1.md,
// "Desktop command").
namespace qindaqt::link_mirror {

enum class InputStepKind { Tap, Scroll, Text, Key, Resize };

// One scripted input for --grab evidence, expressed in window coordinates.
struct InputStep {
    InputStepKind kind = InputStepKind::Tap;
    QPointF position;
    int scrollNotches = 0;
    QString text;
    int qtKey = 0;
    QSize size; // Resize: new window size
};

struct MirrorOptions {
    QString serial;
    QString packageName;
    QString adbPath;
    QString serverJar;
    QSize size{432, 900};
    int dpi = 0; // 0: 160 * device pixel ratio
    QString codec = QStringLiteral("h264");
    int bitrateBps = 8'000'000;
    int maxFps = 60;
    DecoderPreference decoder = DecoderPreference::Auto;
    bool verbose = false;
    bool list = false;
    bool listJson = false; // --list-json: one JSON document instead of tab-separated lines (implies list)
    QString grabPath;
    int grabAfterFrames = 30;
    QList<InputStep> inputSteps;
    QString grabAfterInputPath;
    bool probe = false;
    int probeSeconds = 5;
    int connectTimeoutMs = 10000;

    [[nodiscard]] bool headless() const { return list || probe || !grabPath.isEmpty(); }
};

// Parses arguments (excluding Qt's own). Returns nullopt with *error set on bad input.
[[nodiscard]] std::optional<MirrorOptions> parseMirrorOptions(const QStringList &arguments, QString *error);
// Desktop app id for a package; the task list uses it to tell mirrored apps apart.
[[nodiscard]] QString mirrorAppId(const QString &packageName);

} // namespace qindaqt::link_mirror
