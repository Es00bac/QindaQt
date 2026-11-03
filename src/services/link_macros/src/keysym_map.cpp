// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_macros/keysym_map.h>

#include <QtCore/QHash>
#include <QtGui/QKeySequence>

namespace QindaQt::Link::Macros {
namespace {

constexpr qint32 kShift = 0xffe1, kCtrl = 0xffe3, kAlt = 0xffe9, kMeta = 0xffeb;

qint32 keysymForQtKey(Qt::Key key)
{
    static const QHash<int, qint32> special{
        {Qt::Key_Return, 0xff0d}, {Qt::Key_Enter, 0xff8d}, {Qt::Key_Escape, 0xff1b}, {Qt::Key_Tab, 0xff09},
        {Qt::Key_Backtab, 0xff09}, {Qt::Key_Backspace, 0xff08}, {Qt::Key_Delete, 0xffff}, {Qt::Key_Insert, 0xff63},
        {Qt::Key_Home, 0xff50}, {Qt::Key_Left, 0xff51}, {Qt::Key_Up, 0xff52}, {Qt::Key_Right, 0xff53},
        {Qt::Key_Down, 0xff54}, {Qt::Key_PageUp, 0xff55}, {Qt::Key_PageDown, 0xff56}, {Qt::Key_End, 0xff57},
        {Qt::Key_Print, 0xff61}, {Qt::Key_Pause, 0xff13}, {Qt::Key_ScrollLock, 0xff14}, {Qt::Key_CapsLock, 0xffe5},
        {Qt::Key_NumLock, 0xff7f}, {Qt::Key_Menu, 0xff67}, {Qt::Key_Meta, kMeta}, {Qt::Key_Super_L, kMeta},
        {Qt::Key_Control, kCtrl}, {Qt::Key_Alt, kAlt}, {Qt::Key_Shift, kShift},
        {Qt::Key_VolumeUp, 0x1008ff13}, {Qt::Key_VolumeDown, 0x1008ff11}, {Qt::Key_VolumeMute, 0x1008ff12},
        {Qt::Key_MediaPlay, 0x1008ff14}, {Qt::Key_MediaStop, 0x1008ff15}, {Qt::Key_MediaPrevious, 0x1008ff16},
        {Qt::Key_MediaNext, 0x1008ff17}};
    const auto it = special.constFind(int(key));
    if (it != special.constEnd()) return it.value();
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) return 0xffbe + (int(key) - int(Qt::Key_F1));
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return qint32(key) + 0x20; // shortcut letters are lowercase keysyms
    if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) return qint32(key);
    return 0;
}

} // namespace

qint32 modifierKeysym(const QString &name)
{
    if (name == QLatin1String("Ctrl")) return kCtrl;
    if (name == QLatin1String("Alt")) return kAlt;
    if (name == QLatin1String("Shift")) return kShift;
    if (name == QLatin1String("Meta")) return kMeta;
    return 0;
}

QList<qint32> keysymsForChord(const QString &text)
{
    const QKeySequence seq = QKeySequence::fromString(text, QKeySequence::PortableText);
    if (seq.count() != 1) return {};
    const QKeyCombination combo = seq[0];
    QList<qint32> out;
    const auto mods = combo.keyboardModifiers();
    const bool backtab = combo.key() == Qt::Key_Backtab;
    if (mods.testFlag(Qt::ControlModifier)) out.append(kCtrl);
    if (mods.testFlag(Qt::AltModifier)) out.append(kAlt);
    if (mods.testFlag(Qt::ShiftModifier) || backtab) out.append(kShift);
    if (mods.testFlag(Qt::MetaModifier)) out.append(kMeta);
    const qint32 sym = keysymForQtKey(combo.key());
    if (sym == 0) return {};
    if (!out.contains(sym)) out.append(sym);
    return out;
}

QString defaultChordForAction(const QString &action)
{
    static const QHash<QString, QString> keys{
        {QStringLiteral("accept"), QStringLiteral("Return")}, {QStringLiteral("back"), QStringLiteral("Esc")},
        {QStringLiteral("up"), QStringLiteral("Up")}, {QStringLiteral("down"), QStringLiteral("Down")},
        {QStringLiteral("left"), QStringLiteral("Left")}, {QStringLiteral("right"), QStringLiteral("Right")},
        {QStringLiteral("overview"), QStringLiteral("Meta+W")}, {QStringLiteral("launcher"), QStringLiteral("Meta")},
        {QStringLiteral("next-window"), QStringLiteral("Alt+Tab")},
        {QStringLiteral("previous-window"), QStringLiteral("Alt+Shift+Tab")},
        {QStringLiteral("desktop-left"), QStringLiteral("Meta+Ctrl+Left")},
        {QStringLiteral("desktop-right"), QStringLiteral("Meta+Ctrl+Right")},
        {QStringLiteral("screenshot"), QStringLiteral("Print")}};
    return keys.value(action);
}

} // namespace QindaQt::Link::Macros
