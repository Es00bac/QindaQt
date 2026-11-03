// SPDX-License-Identifier: GPL-3.0-or-later
#include "mirror_options.h"

#include <QCommandLineParser>
#include <QRegularExpression>

namespace qindaqt::link_mirror {
namespace {

std::optional<QSize> parseSize(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char('x'));
    bool okW = false;
    bool okH = false;
    const int width = parts.value(0).toInt(&okW);
    const int height = parts.value(1).toInt(&okH);
    if (parts.size() != 2 || !okW || !okH || width < 64 || height < 64 || width > 8192 || height > 8192) {
        return std::nullopt;
    }
    return QSize(width, height);
}

std::optional<int> namedKey(const QString &name)
{
    static const QHash<QString, int> keys{
        {QStringLiteral("back"), Qt::Key_Back},       {QStringLiteral("enter"), Qt::Key_Return},
        {QStringLiteral("tab"), Qt::Key_Tab},         {QStringLiteral("escape"), Qt::Key_Escape},
        {QStringLiteral("backspace"), Qt::Key_Backspace}, {QStringLiteral("up"), Qt::Key_Up},
        {QStringLiteral("down"), Qt::Key_Down},       {QStringLiteral("left"), Qt::Key_Left},
        {QStringLiteral("right"), Qt::Key_Right},
    };
    const auto it = keys.constFind(name);
    if (it == keys.cend()) {
        return std::nullopt;
    }
    return *it;
}

// tap:X,Y | scroll:X,Y,NOTCHES | text:STRING | key:NAME | resize:WxH (window coordinates)
std::optional<InputStep> parseStep(const QString &spec)
{
    const qsizetype colon = spec.indexOf(QLatin1Char(':'));
    const QString kind = spec.left(colon);
    const QString rest = colon < 0 ? QString() : spec.mid(colon + 1);
    InputStep step;
    if (kind == QLatin1String("resize")) {
        const auto size = parseSize(rest);
        if (!size) {
            return std::nullopt;
        }
        step.kind = InputStepKind::Resize;
        step.size = *size;
        return step;
    }
    if (kind == QLatin1String("text") && !rest.isEmpty()) {
        step.kind = InputStepKind::Text;
        step.text = rest;
        return step;
    }
    if (kind == QLatin1String("key")) {
        const auto key = namedKey(rest);
        if (!key) {
            return std::nullopt;
        }
        step.kind = InputStepKind::Key;
        step.qtKey = *key;
        return step;
    }
    const QStringList numbers = rest.split(QLatin1Char(','));
    bool okX = false;
    bool okY = false;
    step.position = QPointF(numbers.value(0).toDouble(&okX), numbers.value(1).toDouble(&okY));
    if (!okX || !okY) {
        return std::nullopt;
    }
    if (kind == QLatin1String("tap") && numbers.size() == 2) {
        step.kind = InputStepKind::Tap;
        return step;
    }
    bool okN = false;
    step.scrollNotches = numbers.value(2).toInt(&okN);
    if (kind == QLatin1String("scroll") && numbers.size() == 3 && okN && step.scrollNotches != 0) {
        step.kind = InputStepKind::Scroll;
        return step;
    }
    return std::nullopt;
}

bool positiveInt(const QString &text, int min, int max, int *out)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    if (!ok || value < min || value > max) {
        return false;
    }
    *out = value;
    return true;
}

} // namespace

QString mirrorAppId(const QString &packageName)
{
    return QStringLiteral("org.qindaqt.link.mirror.") + packageName;
}

std::optional<MirrorOptions> parseMirrorOptions(const QStringList &arguments, QString *error)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Show one phone app in a desktop window over ADB."));
    parser.addHelpOption();
    const QCommandLineOption serial(QStringLiteral("serial"), QStringLiteral("ADB serial of the phone."), QStringLiteral("serial"));
    const QCommandLineOption package(QStringLiteral("package"), QStringLiteral("Android package to mirror."), QStringLiteral("package"));
    const QCommandLineOption adb(QStringLiteral("adb"), QStringLiteral("adb executable."), QStringLiteral("path"));
    const QCommandLineOption jar(QStringLiteral("server-jar"), QStringLiteral("Development server jar to push."), QStringLiteral("file"));
    const QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Initial window size."), QStringLiteral("WxH"));
    const QCommandLineOption dpi(QStringLiteral("dpi"), QStringLiteral("Virtual display density."), QStringLiteral("dpi"));
    const QCommandLineOption codec(QStringLiteral("codec"), QStringLiteral("h264 or hevc."), QStringLiteral("codec"));
    const QCommandLineOption bitrate(QStringLiteral("bitrate"), QStringLiteral("Encoder bit rate."), QStringLiteral("bps"));
    const QCommandLineOption maxFps(QStringLiteral("max-fps"), QStringLiteral("Encoder frame-rate cap."), QStringLiteral("fps"));
    const QCommandLineOption decoder(QStringLiteral("decoder"), QStringLiteral("auto, vaapi or software."), QStringLiteral("path"));
    const QCommandLineOption verbose(QStringLiteral("verbose"), QStringLiteral("Print server and decoder diagnostics."));
    const QCommandLineOption list(QStringLiteral("list"), QStringLiteral("Print launchable apps and exit."));
    const QCommandLineOption listJson(QStringLiteral("list-json"), QStringLiteral("Print launchable apps as one JSON document and exit."));
    const QCommandLineOption grab(QStringLiteral("grab"), QStringLiteral("Render offscreen, write one frame and exit."), QStringLiteral("png"));
    const QCommandLineOption grabFrames(QStringLiteral("grab-after-frames"), QStringLiteral("Frames to wait before grabbing."), QStringLiteral("n"));
    const QCommandLineOption input(QStringLiteral("input"), QStringLiteral("After --grab: tap:X,Y scroll:X,Y,N text:S key:NAME resize:WxH."), QStringLiteral("step"));
    const QCommandLineOption grabAfter(QStringLiteral("grab-after-input"), QStringLiteral("Second grab after --input steps."), QStringLiteral("png"));
    const QCommandLineOption probe(QStringLiteral("probe"), QStringLiteral("Print decoder path, size, frames and fps, then exit."));
    const QCommandLineOption probeSeconds(QStringLiteral("probe-seconds"), QStringLiteral("Probe duration."), QStringLiteral("s"));
    parser.addOptions({serial, package, adb, jar, size, dpi, codec, bitrate, maxFps, decoder, verbose, list, listJson, grab,
                       grabFrames, input, grabAfter, probe, probeSeconds});
    parser.process(arguments);

    MirrorOptions options;
    options.serial = parser.value(serial);
    options.packageName = parser.value(package);
    options.adbPath = parser.value(adb);
    options.serverJar = parser.value(jar);
    options.verbose = parser.isSet(verbose);
    options.listJson = parser.isSet(listJson);
    options.list = parser.isSet(list) || options.listJson;
    options.probe = parser.isSet(probe);
    options.grabPath = parser.value(grab);
    options.grabAfterInputPath = parser.value(grabAfter);
    static const QRegularExpression serialShape(QStringLiteral("^[A-Za-z0-9._:-]+$"));
    static const QRegularExpression packageShape(QStringLiteral("^[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z0-9_]+)+$"));
    if (!serialShape.match(options.serial).hasMatch()) {
        *error = QStringLiteral("--serial is required");
        return std::nullopt;
    }
    if (!options.list && !packageShape.match(options.packageName).hasMatch()) {
        *error = QStringLiteral("--package must name an Android package");
        return std::nullopt;
    }
    if (parser.isSet(size)) {
        const auto parsed = parseSize(parser.value(size));
        if (!parsed) {
            *error = QStringLiteral("--size must be WxH between 64 and 8192");
            return std::nullopt;
        }
        options.size = *parsed;
    }
    if ((parser.isSet(dpi) && !positiveInt(parser.value(dpi), 72, 960, &options.dpi))
        || (parser.isSet(bitrate) && !positiveInt(parser.value(bitrate), 500'000, 100'000'000, &options.bitrateBps))
        || (parser.isSet(maxFps) && !positiveInt(parser.value(maxFps), 1, 120, &options.maxFps))
        || (parser.isSet(grabFrames) && !positiveInt(parser.value(grabFrames), 1, 10'000, &options.grabAfterFrames))
        || (parser.isSet(probeSeconds) && !positiveInt(parser.value(probeSeconds), 1, 600, &options.probeSeconds))) {
        *error = QStringLiteral("a numeric option is out of range");
        return std::nullopt;
    }
    if (parser.isSet(codec)) {
        options.codec = parser.value(codec);
        if (options.codec != QLatin1String("h264") && options.codec != QLatin1String("hevc")) {
            *error = QStringLiteral("--codec must be h264 or hevc");
            return std::nullopt;
        }
    }
    if (parser.isSet(decoder)) {
        const auto preference = parseDecoderPreference(parser.value(decoder));
        if (!preference) {
            *error = QStringLiteral("--decoder must be auto, vaapi or software");
            return std::nullopt;
        }
        options.decoder = *preference;
    }
    for (const QString &spec : parser.values(input)) {
        const auto step = parseStep(spec);
        if (!step) {
            *error = QStringLiteral("bad --input step: use tap:X,Y scroll:X,Y,N text:S key:NAME or resize:WxH");
            return std::nullopt;
        }
        options.inputSteps.append(*step);
    }
    if (!options.inputSteps.isEmpty() && (options.grabPath.isEmpty() || options.grabAfterInputPath.isEmpty())) {
        *error = QStringLiteral("--input needs --grab and --grab-after-input");
        return std::nullopt;
    }
    return options;
}

} // namespace qindaqt::link_mirror
