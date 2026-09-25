// SPDX-License-Identifier: GPL-3.0-or-later
#include "compat_db_rules.h"

#include "game.h"

#include <QSet>
#include <QTimeZone>

#include <array>
#include <utility>

namespace QindaQt::QindaLutris {
namespace {

bool isAsciiLower(QChar ch) { return ch >= QLatin1Char('a') && ch <= QLatin1Char('z'); }
bool isAsciiUpper(QChar ch) { return ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'); }
bool isAsciiDigit(QChar ch) { return ch >= QLatin1Char('0') && ch <= QLatin1Char('9'); }
bool isAsciiAlnum(QChar ch) { return isAsciiLower(ch) || isAsciiUpper(ch) || isAsciiDigit(ch); }

bool inSet(QChar ch, const char *extra) {
  for (const char *p = extra; *p != '\0'; ++p) {
    if (ch == QLatin1Char(*p)) return true;
  }
  return false;
}

// Whole-string charset match: `first` for position 0, `rest` for the others.
template <typename First, typename Rest>
bool charset(const QString &text, int minChars, int maxChars, First first, Rest rest) {
  if (text.size() < minChars || text.size() > maxChars) return false;
  for (qsizetype i = 0; i < text.size(); ++i) {
    if (!(i == 0 ? first(text.at(i)) : rest(text.at(i)))) return false;
  }
  return true;
}

template <typename Enum, std::size_t N>
std::optional<Enum> lookupId(const std::array<std::pair<const char *, Enum>, N> &table,
                             const QString &id) {
  for (const auto &[name, value] : table) {
    if (id == QLatin1String(name)) return value;
  }
  return std::nullopt;
}

constexpr std::array<std::pair<const char *, CompatStore>, 9> kStores{{
    {"egs", CompatStore::Egs},
    {"gog", CompatStore::Gog},
    {"amazon", CompatStore::Amazon},
    {"ubisoft", CompatStore::Ubisoft},
    {"ea", CompatStore::Ea},
    {"battlenet", CompatStore::Battlenet},
    {"humble", CompatStore::Humble},
    {"itchio", CompatStore::Itchio},
    {"zoomplatform", CompatStore::ZoomPlatform},
}};

} // namespace

QString compatStoreId(CompatStore store) {
  for (const auto &[name, value] : kStores) {
    if (value == store) return QLatin1String(name);
  }
  Q_UNREACHABLE();
}

std::optional<CompatStore> compatStoreForId(const QString &id) {
  return lookupId(kStores, id);
}

bool isValidCompatBuildName(const QString &name) {
  // Directory names such as "GE-Proton11-6-x86_64" or Valve's
  // "Proton 9.0 (Beta)": no path separator, no leading dot, no trailing space.
  return charset(name, 1, kMaxCompatBuildNameChars, isAsciiAlnum,
                 [](QChar ch) { return isAsciiAlnum(ch) || inSet(ch, " ._+()-"); })
      && !name.endsWith(QLatin1Char(' '));
}

namespace CompatRules {

bool isText(const QString &text, int maxChars, bool allowEmpty) {
  if (text.size() > maxChars || (!allowEmpty && text.isEmpty())) return false;
  for (const QChar ch : text) {
    if (ch.category() == QChar::Other_Control) return false;
  }
  return true;
}

bool isGameId(const QString &text) {
  return charset(text, 1, kMaxCompatIdChars,
                 [](QChar ch) { return isAsciiLower(ch) || isAsciiDigit(ch); },
                 [](QChar ch) {
                   return isAsciiLower(ch) || isAsciiDigit(ch) || inSet(ch, "._:-");
                 });
}

bool isSourceId(const QString &text) {
  return charset(text, 1, 64,
                 [](QChar ch) { return isAsciiLower(ch) || isAsciiDigit(ch); },
                 [](QChar ch) { return isAsciiLower(ch) || isAsciiDigit(ch) || ch == u'-'; });
}

bool isUmuId(const QString &text) {
  if (!text.startsWith(QLatin1String("umu-"))) return false;
  const auto ok = [](QChar ch) { return isAsciiAlnum(ch) || inSet(ch, "._-"); };
  return charset(text.mid(4), 1, 124, ok, ok);
}

bool isSteamAppId(const QString &text) {
  return charset(text, 1, 10, [](QChar ch) { return isAsciiDigit(ch) && ch != u'0'; },
                 isAsciiDigit);
}

bool isStoreId(const QString &text) {
  const auto ok = [](QChar ch) { return isAsciiAlnum(ch) || inSet(ch, "._:-"); };
  return charset(text, 1, 128, ok, ok);
}

bool isExeName(const QString &text) {
  return isText(text, 128, false) && !text.contains(QLatin1Char('/'))
      && !text.contains(QLatin1Char('\\'));
}

bool isWinetricksVerb(const QString &text) {
  const auto ok = [](QChar ch) {
    return isAsciiLower(ch) || isAsciiDigit(ch) || inSet(ch, "_=.-");
  };
  if (!charset(text, 1, kMaxCompatVerbChars, ok, ok)) return false;
  // AGENT-CONTRACT: the allowlist is tools/qindalutris-compat/
  // winetricks-verbs.txt, turned into compat_winetricks_verbs.inc by
  // core/CMakeLists.txt at configure time; qlcompat/schema_rules.py reads the
  // same file. Never add verbs here by hand.
  static const QSet<QString> kVerbs = [] {
    static constexpr const char *kList[] = {
#include "compat_winetricks_verbs.inc"
    };
    QSet<QString> verbs;
    for (const char *verb : kList) verbs.insert(QLatin1String(verb));
    return verbs;
  }();
  return kVerbs.contains(text);
}

bool isHttpsUrl(const QString &text) {
  // ^https://[A-Za-z0-9.-]+(:[0-9]{1,5})?([/?#][!-~]*)?$ -- printable ASCII
  // only, so no whitespace, controls or look-alike characters.
  const QLatin1String scheme("https://");
  if (text.size() > kMaxCompatUrlChars || !text.startsWith(scheme)) return false;
  qsizetype i = scheme.size();
  const qsizetype hostStart = i;
  while (i < text.size() && (isAsciiAlnum(text.at(i)) || inSet(text.at(i), ".-"))) ++i;
  if (i == hostStart) return false;
  if (i < text.size() && text.at(i) == u':') {
    const qsizetype portStart = ++i;
    while (i < text.size() && isAsciiDigit(text.at(i))) ++i;
    if (i == portStart || i - portStart > 5) return false;
  }
  if (i == text.size()) return true;
  if (!inSet(text.at(i), "/?#")) return false;
  for (; i < text.size(); ++i) {
    if (text.at(i) < QLatin1Char('!') || text.at(i) > QLatin1Char('~')) return false;
  }
  return true;
}

bool isUmuStore(const QString &text) {
  static constexpr std::array<const char *, 12> kUmuStores{
      "amazon", "battlenet", "ea", "egs", "gog", "humble", "itchio", "steam",
      "ubisoft", "umu", "zoomplatform", "none"};
  for (const char *store : kUmuStores) {
    if (text == QLatin1String(store)) return true;
  }
  return false;
}

bool isCompatEnvironmentKey(const QString &key) {
  // AGENT-GUARD: an ALLOWLIST of EXACT, case-sensitive names -- no prefix
  // families -- mirrored by ENV_EXACT in tools/qindalutris-compat/qlcompat/
  // schema_rules.py. PROTON_* flags were checked against GE-Proton11-6/11-7's
  // `proton` script, __GL_* against NVIDIA's driver README, DXVK_* against
  // upstream DXVK's getEnvVar() calls (DXVK_CONFIG sets inline options, none
  // taking a path), VKD3D_* against vkd3d-proton's vkd3d_get_env_var() calls;
  // all are value-only. Never add a variable that takes a path or file
  // (DXVK_LOG_PATH, VKD3D_SHADER_OVERRIDE, VKD3D_QA_HASHES,
  // VKD3D_QUEUE_PROFILE) or forwards environment (VKD3D_UNIX_ENV and
  // VKD3D_UNIX_POST_ENV set arbitrary Linux variables through ntdll), and
  // never the planner's own (PROTONPATH, WINEPREFIX, GAMEID, STORE, UMU_*).
  // Any key not listed refuses the whole document, so a refreshed download
  // can neither re-pin a title nor make the launch run code of its choosing.
  static constexpr std::array<const char *, 57> kExact{
      "WINEDLLOVERRIDES", "WINE_FULLSCREEN_FSR", "WINE_FULLSCREEN_FSR_STRENGTH",
      "WINE_FULLSCREEN_FSR_MODE", "RADV_PERFTEST", "mesa_glthread",
      "STAGING_SHARED_MEMORY",
      "PROTON_NO_WM_DECORATION", "PROTON_USE_WINED3D", "PROTON_USE_WINED3D11",
      "PROTON_NO_ESYNC", "PROTON_NO_FSYNC", "PROTON_NO_NTSYNC",
      "PROTON_FORCE_LARGE_ADDRESS_AWARE", "PROTON_HIDE_NVIDIA_GPU",
      "PROTON_HIDE_INTEL_GPU", "PROTON_ENABLE_WAYLAND", "PROTON_USE_XALIA",
      "PROTON_PREFER_SDL", "PROTON_ENABLE_HDR", "PROTON_DISABLE_NVAPI",
      "PROTON_FORCE_NVAPI", "PROTON_NO_D3D10", "PROTON_NO_D3D11", "PROTON_DXVK_D3D8",
      "PROTON_HEAP_DELAY_FREE", "PROTON_HEAP_ZERO_MEMORY", "PROTON_OLD_GL_STRING",
      "PROTON_NO_XIM", "PROTON_SET_GAME_DRIVE",
      "__GL_SHADER_DISK_CACHE", "__GL_SHADER_DISK_CACHE_SIZE",
      "__GL_THREADED_OPTIMIZATIONS", "__GL_SYNC_TO_VBLANK", "__GL_VRR_ALLOWED",
      "__GL_YIELD", "__GL_FSAA_MODE", "__GL_SHARPEN_ENABLE", "__GL_SHARPEN_VALUE",
      "__GL_ALLOW_FXAA_USAGE",
      "DXVK_HUD", "DXVK_CONFIG", "DXVK_ENABLE_NVAPI", "DXVK_FILTER_DEVICE_NAME",
      "DXVK_FILTER_DEVICE_UUID", "DXVK_HDR", "DXVK_SHADER_CACHE", "DXVK_FORCE_WINDOWED",
      "DXVK_NO_VR",
      "VKD3D_CONFIG", "VKD3D_FEATURE_LEVEL", "VKD3D_FRAME_RATE",
      "VKD3D_SWAPCHAIN_LATENCY_FRAMES", "VKD3D_SWAPCHAIN_PRESENT_MODE",
      "VKD3D_FILTER_DEVICE_NAME", "VKD3D_VULKAN_DEVICE", "VKD3D_DISABLE_EXTENSIONS"};
  for (const char *exact : kExact) {
    if (key == QLatin1String(exact)) return true;
  }
  return false;
}

bool isCompatEnvironmentAssignment(const QString &line) {
  if (!isText(line, kMaxEnvironmentValueChars + 65, false)) return false;
  const qsizetype equals = line.indexOf(QLatin1Char('='));
  if (equals <= 0) return false;
  const QString key = line.left(equals);
  const auto keyFirst = [](QChar ch) { return isAsciiUpper(ch) || isAsciiLower(ch) || ch == u'_'; };
  const auto keyRest = [](QChar ch) { return isAsciiAlnum(ch) || ch == u'_'; };
  if (!charset(key, 1, 64, keyFirst, keyRest) || !isCompatEnvironmentKey(key)) return false;
  const QStringView value = QStringView(line).sliced(equals + 1);
  return !value.contains(u'$') && !value.contains(u'`');
}

QString compatEnvironmentKey(const QString &line) {
  return line.left(line.indexOf(QLatin1Char('=')));
}

QDateTime parseTimestamp(const QString &text) {
  // Positions are checked by hand so no locale, sign or padding leniency of
  // the Qt date parsers can widen the accepted set beyond the Python twin.
  static constexpr QLatin1StringView kShape("dddd-dd-ddTdd:dd:ddZ");
  if (text.size() != kShape.size()) return {};
  for (qsizetype i = 0; i < text.size(); ++i) {
    const bool ok = kShape.at(i) == QLatin1Char('d') ? isAsciiDigit(text.at(i))
                                                     : text.at(i) == kShape.at(i);
    if (!ok) return {};
  }
  const QDate date = QDate::fromString(text.left(10), QStringLiteral("yyyy-MM-dd"));
  const QTime time = QTime::fromString(text.mid(11, 8), QStringLiteral("HH:mm:ss"));
  if (!date.isValid() || !time.isValid()) return {};
  return QDateTime(date, time, QTimeZone::UTC);
}

std::optional<CompatBuildStatus> buildStatusForId(const QString &id) {
  static constexpr std::array<std::pair<const char *, CompatBuildStatus>, 3> kTable{{
      {"tested", CompatBuildStatus::Tested},
      {"known-issues", CompatBuildStatus::KnownIssues},
      {"untested", CompatBuildStatus::Untested},
  }};
  return lookupId(kTable, id);
}

std::optional<AntiCheatStatus> antiCheatStatusForId(const QString &id) {
  static constexpr std::array<std::pair<const char *, AntiCheatStatus>, 7> kTable{{
      {"supported", AntiCheatStatus::Supported},
      {"running", AntiCheatStatus::Running},
      {"broken", AntiCheatStatus::Broken},
      {"denied", AntiCheatStatus::Denied},
      {"planned", AntiCheatStatus::Planned},
      {"none", AntiCheatStatus::None},
      {"unknown", AntiCheatStatus::Unknown},
  }};
  return lookupId(kTable, id);
}

std::optional<ProtonDbTier> protonDbTierForId(const QString &id) {
  static constexpr std::array<std::pair<const char *, ProtonDbTier>, 8> kTable{{
      {"platinum", ProtonDbTier::Platinum},
      {"gold", ProtonDbTier::Gold},
      {"silver", ProtonDbTier::Silver},
      {"bronze", ProtonDbTier::Bronze},
      {"borked", ProtonDbTier::Borked},
      {"native", ProtonDbTier::Native},
      {"pending", ProtonDbTier::Pending},
      {"unknown", ProtonDbTier::Unknown},
  }};
  return lookupId(kTable, id);
}

std::optional<SteamDeckCategory> steamDeckCategoryForId(const QString &id) {
  static constexpr std::array<std::pair<const char *, SteamDeckCategory>, 4> kTable{{
      {"verified", SteamDeckCategory::Verified},
      {"playable", SteamDeckCategory::Playable},
      {"unsupported", SteamDeckCategory::Unsupported},
      {"unknown", SteamDeckCategory::Unknown},
  }};
  return lookupId(kTable, id);
}

} // namespace CompatRules
} // namespace QindaQt::QindaLutris
