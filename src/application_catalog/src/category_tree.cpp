// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/application_catalog/category_tree.h"

#include "qindaqt/shell_launcher/launcher_category_model.h"

#include <QHash>

#include <algorithm>
#include <array>

namespace QindaQt::ApplicationCatalog {
namespace {

using QindaQt::ShellLauncher::LauncherCategory;
using QindaQt::ShellLauncher::LauncherCategoryModel;

struct MainGroupSpec final
{
    LauncherCategory category;
    const char *id;
    const char *label;
};

// Same groups, order, and locale-independent labels as the shell launcher's
// presentation; keep the two lists consistent when either changes.
constexpr std::array<MainGroupSpec, 12> mainGroups{{
    {LauncherCategory::Utilities, "Utilities", "Utilities"},
    {LauncherCategory::Development, "Development", "Development"},
    {LauncherCategory::Education, "Education", "Education"},
    {LauncherCategory::Games, "Games", "Games"},
    {LauncherCategory::Graphics, "Graphics", "Graphics"},
    {LauncherCategory::AudioVideo, "AudioVideo", "Sound & Video"},
    {LauncherCategory::Network, "Network", "Internet"},
    {LauncherCategory::Office, "Office", "Office"},
    {LauncherCategory::Science, "Science", "Science"},
    {LauncherCategory::Settings, "Settings", "Settings"},
    {LauncherCategory::System, "System", "System"},
    {LauncherCategory::Other, "Other", "Other"},
}};

// The Desktop Menu Specification's registered additional categories. Only
// tokens on this list become child folders; unrecognized vendor categories
// are ignored for nesting so hostile or noisy entries cannot spam the tree.
constexpr std::array<QLatin1String, 96> registeredAdditionalCategories{{
    QLatin1String("Building"), QLatin1String("Debugger"), QLatin1String("IDE"),
    QLatin1String("GUIDesigner"), QLatin1String("Profiling"),
    QLatin1String("RevisionControl"), QLatin1String("Translation"),
    QLatin1String("Calendar"), QLatin1String("ContactManagement"),
    QLatin1String("Dictionary"), QLatin1String("Chart"), QLatin1String("Email"),
    QLatin1String("Finance"), QLatin1String("FlowChart"), QLatin1String("PDA"),
    QLatin1String("ProjectManagement"), QLatin1String("Presentation"),
    QLatin1String("Spreadsheet"), QLatin1String("WordProcessor"),
    QLatin1String("2DGraphics"), QLatin1String("VectorGraphics"),
    QLatin1String("RasterGraphics"), QLatin1String("3DGraphics"),
    QLatin1String("Scanning"), QLatin1String("OCR"),
    QLatin1String("Photography"), QLatin1String("Publisher"),
    QLatin1String("Viewer"), QLatin1String("TextEditor"),
    QLatin1String("DesktopSettings"), QLatin1String("HardwareSettings"),
    QLatin1String("Printing"), QLatin1String("PackageManager"),
    QLatin1String("Dialup"), QLatin1String("InstantMessaging"),
    QLatin1String("Chat"), QLatin1String("IRCClient"), QLatin1String("Feed"),
    QLatin1String("FileTransfer"), QLatin1String("HamRadio"),
    QLatin1String("News"), QLatin1String("P2P"), QLatin1String("RemoteAccess"),
    QLatin1String("Telephony"), QLatin1String("TelephonyTools"),
    QLatin1String("VideoConference"), QLatin1String("WebBrowser"),
    QLatin1String("WebDevelopment"), QLatin1String("Midi"),
    QLatin1String("Mixer"), QLatin1String("Sequencer"), QLatin1String("Tuner"),
    QLatin1String("TV"), QLatin1String("AudioVideoEditing"),
    QLatin1String("Player"), QLatin1String("Recorder"),
    QLatin1String("DiscBurning"), QLatin1String("ActionGame"),
    QLatin1String("AdventureGame"), QLatin1String("ArcadeGame"),
    QLatin1String("BoardGame"), QLatin1String("BlocksGame"),
    QLatin1String("CardGame"), QLatin1String("KidsGame"),
    QLatin1String("LogicGame"), QLatin1String("RolePlaying"),
    QLatin1String("Shooter"), QLatin1String("Simulation"),
    QLatin1String("SportsGame"), QLatin1String("StrategyGame"),
    QLatin1String("Art"), QLatin1String("Construction"),
    QLatin1String("Music"), QLatin1String("Languages"),
    QLatin1String("Mathematics"), QLatin1String("NumericalAnalysis"),
    QLatin1String("DataVisualization"), QLatin1String("Economy"),
    QLatin1String("Electricity"), QLatin1String("Geography"),
    QLatin1String("Geology"), QLatin1String("Geoscience"),
    QLatin1String("History"), QLatin1String("Humanities"),
    QLatin1String("ImageProcessing"), QLatin1String("Literature"),
    QLatin1String("Maps"), QLatin1String("Spirituality"),
    QLatin1String("Biology"), QLatin1String("Chemistry"),
    QLatin1String("Physics"), QLatin1String("ArtificialIntelligence"),
    QLatin1String("Robotics"), QLatin1String("Engineering"),
    QLatin1String("Astronomy"),
}};

bool isRegisteredAdditional(const QString &token)
{
    return std::any_of(registeredAdditionalCategories.begin(),
                       registeredAdditionalCategories.end(),
                       [&token](QLatin1String candidate) {
                           return token == candidate;
                       });
}

// "StrategyGame" -> "Strategy Game", "2DGraphics" -> "2D Graphics",
// "IRCClient" -> "IRC Client". Locale-independent, deterministic.
QString humanizeCategoryToken(const QString &token)
{
    QString label;
    label.reserve(token.size() + 4);
    for (qsizetype index = 0; index < token.size(); ++index) {
        const QChar character = token.at(index);
        const bool previousWasLowerOrDigit = index > 0
            && (token.at(index - 1).isLower()
                || token.at(index - 1).isDigit());
        if (index > 0 && character.isUpper() && previousWasLowerOrDigit) {
            label.append(QLatin1Char(' '));
        }
        label.append(character);
    }
    return label;
}

struct Assignment final
{
    LauncherCategory primary = LauncherCategory::Other;
    QString additionalToken;
};

Assignment assignmentFor(const ApplicationEntry &entry)
{
    Assignment assignment;
    assignment.primary = LauncherCategoryModel::categoryFor(entry.categories);
    for (const auto &token : entry.categories) {
        if (isRegisteredAdditional(token)) {
            // First registered additional category wins, mirroring the
            // first-main-category-wins rule of the flat grouping.
            assignment.additionalToken = token;
            break;
        }
    }
    return assignment;
}

} // namespace

CategoryNode buildCategoryTree(const QVector<ApplicationEntry> &applications)
{
    // Bucket per main group, then per additional token, so folders appear
    // only for non-empty groups/children regardless of input order.
    struct ChildBucket final
    {
        QString token;
        QVector<ApplicationEntry> entries;
    };
    struct GroupBucket final
    {
        const MainGroupSpec *spec = nullptr;
        QVector<ApplicationEntry> entries;
        QVector<ChildBucket> children;
    };

    QHash<int, qsizetype> bucketByCategory;
    QVector<GroupBucket> buckets;
    buckets.reserve(mainGroups.size());
    for (const auto &spec : mainGroups) {
        bucketByCategory.insert(int(spec.category), buckets.size());
        buckets.append(GroupBucket{&spec, {}, {}});
    }

    for (const auto &entry : applications) {
        const auto assignment = assignmentFor(entry);
        const auto bucketIndex = bucketByCategory.value(int(assignment.primary),
                                                        -1);
        if (bucketIndex < 0) {
            continue;
        }
        auto &bucket = buckets[bucketIndex];
        if (assignment.additionalToken.isEmpty()) {
            bucket.entries.append(entry);
            continue;
        }
        auto child = std::find_if(bucket.children.begin(), bucket.children.end(),
                                  [&assignment](const ChildBucket &candidate) {
                                      return candidate.token
                                          == assignment.additionalToken;
                                  });
        if (child == bucket.children.end()) {
            bucket.children.append(
                ChildBucket{assignment.additionalToken, {}});
            child = bucket.children.end() - 1;
        }
        child->entries.append(entry);
    }

    CategoryNode root;
    root.id = QStringLiteral("root");
    for (auto &bucket : buckets) {
        if (bucket.entries.isEmpty() && bucket.children.isEmpty()) {
            continue;
        }
        CategoryNode group;
        group.id = QLatin1String(bucket.spec->id);
        group.label = QLatin1String(bucket.spec->label);
        group.entries = bucket.entries;
        for (auto &child : bucket.children) {
            CategoryNode childNode;
            childNode.id = child.token;
            childNode.label = humanizeCategoryToken(child.token);
            childNode.entries = child.entries;
            group.children.append(std::move(childNode));
        }
        std::sort(group.children.begin(), group.children.end(),
                  [](const CategoryNode &first, const CategoryNode &second) {
                      return first.label.compare(second.label,
                                                 Qt::CaseInsensitive)
                          < 0;
                  });
        root.children.append(std::move(group));
    }
    return root;
}

} // namespace QindaQt::ApplicationCatalog
