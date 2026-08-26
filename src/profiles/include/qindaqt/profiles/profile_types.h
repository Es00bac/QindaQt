// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Profiles {

enum class Edge { Top, Bottom, Left, Right };
enum class Layer { Below, Normal, Above, Overlay };
enum class HideMode { Never, Intelligent, DodgeActive, DodgeAll, Maximized, Always };
enum class Alignment { Start, Center, End, Fill };

QString toString(Edge value);
QString toString(Layer value);
QString toString(HideMode value);
QString toString(Alignment value);

bool parseEdge(const QString &text, Edge *value);
bool parseLayer(const QString &text, Layer *value);
bool parseHideMode(const QString &text, HideMode *value);
bool parseAlignment(const QString &text, Alignment *value);

} // namespace QindaQt::Profiles
