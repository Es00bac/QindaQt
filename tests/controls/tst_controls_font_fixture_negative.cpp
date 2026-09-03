// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include <QGuiApplication>
#include <QIODevice>
#include <QString>
#include <QtGlobal>

#include <cstdio>

// AGENT-CONTRACT: pinDeterministicFonts() must fail closed. These invocation
// modes each drive one failure branch and expect the process to abort with
// the matching diagnostic; run_controls_font_fixture_negative.cmake asserts
// both the abort and the exact message, so a future edit that turns a broken
// fixture into silent host-font substitution fails these rows.
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <missing|corrupt|wrongfamily> <font-dir>\n", argv[0]);
        return 2;
    }
    const QString mode = QString::fromLocal8Bit(argv[1]);
    const QString fontDir = QString::fromLocal8Bit(argv[2]);
    if (mode != QLatin1String("missing") && mode != QLatin1String("corrupt")
        && mode != QLatin1String("wrongfamily")) {
        std::fprintf(stderr, "unknown mode: %s\n", argv[1]);
        return 2;
    }
    QindaQt::Controls::TestSupport::pinDeterministicFonts(fontDir);
    std::fprintf(stderr, "pinDeterministicFonts returned for mode %s; expected abort\n", argv[1]);
    return 1;
}
