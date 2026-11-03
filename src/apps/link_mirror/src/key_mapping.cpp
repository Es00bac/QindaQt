// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/link_mirror/input_mapper.h"

// Qt key -> android.view.KeyEvent keycode. Letters and digits are mapped so Ctrl/Alt chords
// (copy, paste, select all) reach the app as key events; plain typing goes out as text.
//
// AGENT-NOTE: volume, power, camera and call keys are deliberately unmapped. A mirror window
// must not change phone-wide state such as volume or start a call from a desktop keystroke.
namespace qindaqt::link_mirror {
namespace {

struct KeyPair {
    int qt;
    int android;
};

constexpr KeyPair kSpecialKeys[] = {
    {Qt::Key_Escape, 111},   {Qt::Key_Tab, 61},        {Qt::Key_Backtab, 61},    {Qt::Key_Backspace, 67},
    {Qt::Key_Return, 66},    {Qt::Key_Enter, 160},     {Qt::Key_Insert, 124},    {Qt::Key_Delete, 112},
    {Qt::Key_Home, 122},     {Qt::Key_End, 123},       {Qt::Key_Left, 21},       {Qt::Key_Up, 19},
    {Qt::Key_Right, 22},     {Qt::Key_Down, 20},       {Qt::Key_PageUp, 92},     {Qt::Key_PageDown, 93},
    {Qt::Key_Space, 62},     {Qt::Key_Menu, 82},       {Qt::Key_Back, 4},        {Qt::Key_Comma, 55},
    {Qt::Key_Period, 56},    {Qt::Key_Minus, 69},      {Qt::Key_Equal, 70},      {Qt::Key_BracketLeft, 71},
    {Qt::Key_BracketRight, 72}, {Qt::Key_Backslash, 73}, {Qt::Key_Semicolon, 74}, {Qt::Key_Apostrophe, 75},
    {Qt::Key_Slash, 76},     {Qt::Key_QuoteLeft, 68},
};

} // namespace

int androidKeycode(int qtKey)
{
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        return 29 + (qtKey - Qt::Key_A); // KEYCODE_A .. KEYCODE_Z
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        return 7 + (qtKey - Qt::Key_0); // KEYCODE_0 .. KEYCODE_9
    }
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12) {
        return 131 + (qtKey - Qt::Key_F1); // KEYCODE_F1 .. KEYCODE_F12
    }
    for (const KeyPair &pair : kSpecialKeys) {
        if (pair.qt == qtKey) {
            return pair.android;
        }
    }
    return 0;
}

} // namespace qindaqt::link_mirror
