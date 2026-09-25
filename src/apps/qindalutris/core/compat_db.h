// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the compatibility database `compat-db-v1` (ADR-0275 §3,
// schema in docs/wiki/apps/qindalutris-compat-db.md). Producer:
// tools/qindalutris-compat/generate.py, whose validator
// tools/qindalutris-compat/qlcompat/schema.py enforces the SAME rules as
// compat_db_parse.cpp -- a rule changed on one side must change on the
// other in the same commit, and the committed snapshot
// src/apps/qindalutris/compat/compat-db-v1.json is loaded by both test
// suites to catch drift. A document failing any rule is refused WHOLE: the
// caller gets an empty database ("no advice"), never a partial one.

inline constexpr qint64 kMaxCompatDbBytes = qint64(32) * 1024 * 1024;
inline constexpr int kMaxCompatGames = 100000;
inline constexpr int kMaxCompatSources = 32;
inline constexpr int kMaxCompatBuilds = 256;
inline constexpr int kMaxCompatListEntries = 64;
inline constexpr int kMaxCompatIdChars = 128;
inline constexpr int kMaxCompatBuildNameChars = 128;
inline constexpr int kMaxCompatTextChars = 2048;
inline constexpr int kMaxCompatUrlChars = 2048;
inline constexpr int kMaxCompatArgumentChars = 512;
inline constexpr int kMaxCompatVerbChars = 64;

enum class CompatBuildStatus { Untested, Tested, KnownIssues };
enum class AntiCheatStatus {
  Unknown, None, Supported, Running, Broken, Denied, Planned
};
enum class ProtonDbTier {
  Unknown, Platinum, Gold, Silver, Bronze, Borked, Native, Pending
};
// Valve's Steam Deck compatibility rating (store "Deck Verified" badge).
enum class SteamDeckCategory { Unknown, Verified, Playable, Unsupported };
// The non-Steam stores whose ids key a record. Steam appids are separate
// (CompatGameKeys::steamAppIds) because Steam is a different lookup rank.
enum class CompatStore {
  Egs, Gog, Amazon, Ubisoft, Ea, Battlenet, Humble, Itchio, ZoomPlatform
};
// Which key produced a lookup hit, strongest first (the lookup precedence).
enum class CompatMatch { UmuId, StoreId, SteamAppId, ExeName, Title };

[[nodiscard]] QString compatStoreId(CompatStore store);
[[nodiscard]] std::optional<CompatStore> compatStoreForId(const QString &id);

struct CompatSource {
  QString id;
  QString url;
  QDateTime retrieved; // UTC
  friend bool operator==(const CompatSource &, const CompatSource &) = default;
};

struct CompatBuildInfo {
  CompatBuildStatus status = CompatBuildStatus::Untested;
  QString notes;
  friend bool operator==(const CompatBuildInfo &, const CompatBuildInfo &) = default;
};

struct CompatAvoid {
  QString build;  // exact build directory name, e.g. GE-Proton11-7-x86_64
  QString reason; // plain-language sentence shown before a move
  QString source; // evidence: a URL or a dated observation
  friend bool operator==(const CompatAvoid &, const CompatAvoid &) = default;
};

struct CompatGameKeys {
  QString umuId;                          // "umu-..." or empty
  QStringList steamAppIds;                // decimal appids
  QMap<CompatStore, QStringList> storeIds; // store-native ids/codenames
  QStringList exeNames;                   // basenames, matched case-insensitively
  QStringList titles;                     // aliases, matched normalized
  friend bool operator==(const CompatGameKeys &, const CompatGameKeys &) = default;
};

struct CompatGame {
  QString id; // stable across regenerations
  QString title;
  CompatGameKeys keys;
  QString recommendedBuild; // empty = use the database default
  QVector<CompatAvoid> avoid;
  QStringList environment; // KEY=VALUE, planner-reserved keys refused
  QStringList winetricks;  // verbs, applied via `umu-run winetricks <verbs>`
  QStringList arguments;
  AntiCheatStatus antiCheat = AntiCheatStatus::Unknown;
  QString antiCheatNotes;
  ProtonDbTier protondbTier = ProtonDbTier::Unknown;
  SteamDeckCategory steamDeck = SteamDeckCategory::Unknown;
  QStringList steamDeckNotes; // Valve's failed/advisory test results, readable
  QString umuStore; // umu STORE value; empty = none recorded
  QStringList notes;
  QStringList links; // https only
  friend bool operator==(const CompatGame &, const CompatGame &) = default;
};

// A validated document, before indexing. Produced only by
// parseCompatDocument; exposed so tests and the refresh path can inspect it.
struct CompatDocument {
  QDateTime generated; // UTC, always valid in a parsed document
  QVector<CompatSource> sources;
  QString recommendedBuild; // empty, or a key of `builds`
  QHash<QString, CompatBuildInfo> builds;
  QVector<CompatGame> games;
};

// What the app knows about a title from its own library: any subset may be
// empty. `executable` may be a full Windows or POSIX path; only its basename
// participates.
struct GameKeys {
  QString umuId;
  std::optional<CompatStore> store;
  QString storeId;
  QString steamAppId;
  QString executable;
  QString title;
};

struct CompatAdvice {
  CompatGame game;
  CompatMatch matchedBy = CompatMatch::Title;
};

// An immutable, indexed compatibility database. Value type: cheap to copy
// (implicitly shared Qt containers), safe to read from any thread once
// built. A default-constructed database is the "no advice" database -- what
// every refusal and absence degrades to.
class CompatDatabase final {
public:
  CompatDatabase() = default;

  // Indexes a parsed document. Refuses (nullopt) when two games share a
  // game id, umu id, store id or Steam appid -- the strong keys must name
  // exactly one game or a lookup could silently pick the wrong advice.
  [[nodiscard]] static std::optional<CompatDatabase> fromDocument(
      CompatDocument document);

  [[nodiscard]] bool isLoaded() const { return m_doc.generated.isValid(); }
  [[nodiscard]] QDateTime generated() const { return m_doc.generated; }
  [[nodiscard]] const QVector<CompatSource> &sources() const { return m_doc.sources; }
  [[nodiscard]] const QVector<CompatGame> &games() const { return m_doc.games; }

  // The first-run default build for new installs; empty when none.
  [[nodiscard]] QString recommendedBuild() const { return m_doc.recommendedBuild; }

  // Unknown builds are Untested ("Not tested by QindaQt"), never an error.
  [[nodiscard]] CompatBuildStatus buildStatus(const QString &buildName) const;
  [[nodiscard]] QString buildNotes(const QString &buildName) const;

  // Precedence: umu id > store id > Steam appid > exe basename > normalized
  // title. A key naming more than one game at the exe or title rank is
  // ambiguous and skipped rather than guessed.
  [[nodiscard]] std::optional<CompatAdvice> lookup(const GameKeys &keys) const;

private:
  CompatDocument m_doc;
  QHash<QString, int> m_byUmu;
  QHash<QString, int> m_byStore; // "<store>\x1f<id>"
  QHash<QString, int> m_bySteam;
  QHash<QString, int> m_byExe;   // casefolded; -1 = ambiguous
  QHash<QString, int> m_byTitle; // normalizedTitleForMatch; -1 = ambiguous
};

// The avoid reason for `buildName` in this advice; empty = not avoided.
[[nodiscard]] QString avoidReasonFor(const CompatAdvice &advice,
                                     const QString &buildName);

// The pin for a new install: the game's recommended build, else the
// database default. Empty when the database has neither.
[[nodiscard]] QString recommendedBuildFor(const CompatDatabase &database,
                                          const std::optional<CompatAdvice> &advice);

enum class CompatLoadError { None, Absent, Refused };

// Parses the bytes of one document; nullopt = refused whole.
[[nodiscard]] std::optional<CompatDocument> parseCompatDocument(
    const QByteArray &bytes);

// Reads one document from disk. Refuses a symlink, a non-regular file, and
// a file over kMaxCompatDbBytes before parsing. Blocking file I/O: call off
// the UI thread for large documents.
[[nodiscard]] CompatDatabase loadCompatDatabase(const QString &path,
                                                CompatLoadError *error);

// ADR-0275 §3: the copy with the newer `generated` stamp wins; an unloaded
// (absent or refused) copy never wins; a tie keeps the shipped copy.
[[nodiscard]] CompatDatabase chooseNewer(const CompatDatabase &shipped,
                                         const CompatDatabase &refreshed);

// Loads both copies and applies chooseNewer. Paths are injected.
[[nodiscard]] CompatDatabase loadEffectiveCompatDatabase(
    const QString &shippedPath, const QString &refreshedPath);

// `<datadir>/qindalutris/compat-db-v1.json` of the install prefix the model
// was configured for (normally /usr/share/qindalutris/compat-db-v1.json).
[[nodiscard]] QString shippedCompatDatabasePath();
[[nodiscard]] QString refreshedCompatDatabasePath(const QString &dataHome);
// Uses $XDG_DATA_HOME (QStandardPaths::GenericDataLocation).
[[nodiscard]] QString defaultRefreshedCompatDatabasePath();

// A Proton build directory name as the database spells it.
[[nodiscard]] bool isValidCompatBuildName(const QString &name);

} // namespace QindaQt::QindaLutris
