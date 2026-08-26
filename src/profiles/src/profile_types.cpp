// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/profile_types.h"

namespace QindaQt::Profiles {
namespace {

template<typename Enum>
struct EnumName {
    Enum value;
    const char *name;
};

template<typename Enum, std::size_t Size>
QString enumToString(Enum value, const EnumName<Enum> (&values)[Size])
{
    for (const auto &candidate : values) {
        if (candidate.value == value) {
            return QString::fromLatin1(candidate.name);
        }
    }
    return {};
}

template<typename Enum, std::size_t Size>
bool parseEnum(const QString &text, Enum *value, const EnumName<Enum> (&values)[Size])
{
    if (value == nullptr) {
        return false;
    }
    for (const auto &candidate : values) {
        if (text.compare(QLatin1String(candidate.name), Qt::CaseInsensitive) == 0) {
            *value = candidate.value;
            return true;
        }
    }
    return false;
}

constexpr EnumName<Edge> edges[] = {
    {Edge::Top, "top"}, {Edge::Bottom, "bottom"}, {Edge::Left, "left"}, {Edge::Right, "right"}};
constexpr EnumName<Layer> layers[] = {{Layer::Below, "below"},
                                      {Layer::Normal, "normal"},
                                      {Layer::Above, "above"},
                                      {Layer::Overlay, "overlay"}};
constexpr EnumName<HideMode> hideModes[] = {{HideMode::Never, "never"},
                                            {HideMode::Intelligent, "intelligent"},
                                            {HideMode::DodgeActive, "dodge-active"},
                                            {HideMode::DodgeAll, "dodge-all"},
                                            {HideMode::Maximized, "maximized"},
                                            {HideMode::Always, "always"}};
constexpr EnumName<Alignment> alignments[] = {{Alignment::Start, "start"},
                                              {Alignment::Center, "center"},
                                              {Alignment::End, "end"},
                                              {Alignment::Fill, "fill"}};

} // namespace

QString toString(Edge value) { return enumToString(value, edges); }
QString toString(Layer value) { return enumToString(value, layers); }
QString toString(HideMode value) { return enumToString(value, hideModes); }
QString toString(Alignment value) { return enumToString(value, alignments); }

bool parseEdge(const QString &text, Edge *value) { return parseEnum(text, value, edges); }
bool parseLayer(const QString &text, Layer *value) { return parseEnum(text, value, layers); }
bool parseHideMode(const QString &text, HideMode *value) { return parseEnum(text, value, hideModes); }
bool parseAlignment(const QString &text, Alignment *value) { return parseEnum(text, value, alignments); }

} // namespace QindaQt::Profiles
