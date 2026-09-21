// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/steamappidentity.h"

#include <QRegularExpression>

namespace QindaQt::Compositor {
namespace {

// One parsed VDF pair tree. `children` is empty for a string node. Kept as a
// plain value type so the reader can build it recursively within the pair and
// depth budgets; no node outlives the parse call.
struct VdfNode {
    QString value;
    QList<QPair<QString, VdfNode>> children;
};

class VdfReader
{
public:
    explicit VdfReader(const QByteArray &bytes) : m_bytes(bytes) {}

    // Parses exactly one document and requires it to consume the payload.
    [[nodiscard]] bool parse(VdfNode *root)
    {
        m_pairs = 0;
        if (m_bytes.size() > kMaxVdfBytes) {
            return false;
        }
        if (!parsePairs(root, 0, false)) {
            return false;
        }
        skipWhitespace();
        // Trailing garbage means the file is not what it claims to be.
        return m_pos == m_bytes.size();
    }

private:
    void skipWhitespace()
    {
        while (m_pos < m_bytes.size()) {
            const char c = m_bytes.at(m_pos);
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                break;
            }
            ++m_pos;
        }
    }

    // AGENT-GUARD: strings are the only unbounded-looking production in the
    // grammar; the token cap and the input byte cap together bound them. A
    // backslash escapes the next byte literally (Valve writes `\\` for path
    // separators and `\"` inside names).
    [[nodiscard]] bool readString(QString *out)
    {
        if (m_pos >= m_bytes.size() || m_bytes.at(m_pos) != '"') {
            return false;
        }
        ++m_pos;
        QByteArray token;
        token.reserve(64);
        while (m_pos < m_bytes.size()) {
            char c = m_bytes.at(m_pos++);
            if (c == '"') {
                if (token.size() > kMaxVdfTokenUtf8Bytes) {
                    return false;
                }
                *out = QString::fromUtf8(token);
                return true;
            }
            if (c == '\\') {
                if (m_pos >= m_bytes.size()) {
                    return false;
                }
                c = m_bytes.at(m_pos++);
            }
            token.append(c);
        }
        // Unterminated string.
        return false;
    }

    [[nodiscard]] bool parsePairs(VdfNode *node, int depth, bool expectClose)
    {
        if (depth > kMaxVdfDepth) {
            return false;
        }
        while (true) {
            skipWhitespace();
            if (m_pos >= m_bytes.size()) {
                return !expectClose;
            }
            if (m_bytes.at(m_pos) == '}') {
                ++m_pos;
                return expectClose;
            }
            if (++m_pairs > kMaxVdfPairs) {
                return false;
            }
            QString key;
            if (!readString(&key)) {
                return false;
            }
            skipWhitespace();
            if (m_pos >= m_bytes.size()) {
                return false;
            }
            if (m_bytes.at(m_pos) == '{') {
                ++m_pos;
                VdfNode child;
                if (!parsePairs(&child, depth + 1, true)) {
                    return false;
                }
                node->children.append({key, std::move(child)});
            } else {
                QString value;
                if (!readString(&value)) {
                    return false;
                }
                VdfNode child;
                child.value = value;
                node->children.append({key, std::move(child)});
            }
        }
    }

    const QByteArray &m_bytes;
    qsizetype m_pos = 0;
    int m_pairs = 0;
};

[[nodiscard]] const VdfNode *findChild(const VdfNode &node, const QString &key,
                                       Qt::CaseSensitivity sensitivity)
{
    for (const auto &child : node.children) {
        if (child.first.compare(key, sensitivity) == 0) {
            return &child.second;
        }
    }
    return nullptr;
}

[[nodiscard]] bool isUsableText(const QString &value, int maxCharacters)
{
    if (value.isEmpty() || value.size() > maxCharacters) {
        return false;
    }
    for (const QChar character : value) {
        const char16_t code = character.unicode();
        if (code <= 0x001F || code == 0x007F) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<quint64> steamAppIdFromClass(const QString &resourceClass)
{
    static const QRegularExpression pattern(QStringLiteral("^steam_app_([0-9]+)$"));
    const QRegularExpressionMatch match =
        pattern.match(resourceClass.trimmed().toLower());
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool ok = false;
    const quint64 id = match.captured(1).toULongLong(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return id;
}

bool parseSteamLibraryFolders(const QByteArray &vdf, QStringList *paths)
{
    paths->clear();
    VdfNode root;
    if (!VdfReader(vdf).parse(&root)) {
        return false;
    }
    const VdfNode *folders = findChild(root, QStringLiteral("libraryfolders"),
                                       Qt::CaseInsensitive);
    if (folders == nullptr || !folders->value.isEmpty()) {
        return false;
    }
    for (const auto &entry : folders->children) {
        QString path;
        if (entry.second.children.isEmpty()) {
            // Legacy flat form: the entry value itself is the path.
            path = entry.second.value;
        } else {
            const VdfNode *pathNode =
                findChild(entry.second, QStringLiteral("path"), Qt::CaseSensitive);
            if (pathNode != nullptr) {
                path = pathNode->value;
            }
        }
        if (path.isEmpty()) {
            continue;
        }
        if (!isUsableText(path, kMaxVdfTokenUtf8Bytes)) {
            return false;
        }
        if (paths->size() >= kMaxSteamLibraryRoots) {
            break;
        }
        paths->append(path);
    }
    return true;
}

QString parseSteamAppManifestName(const QByteArray &acf)
{
    VdfNode root;
    if (!VdfReader(acf).parse(&root)) {
        return {};
    }
    const VdfNode *appState =
        findChild(root, QStringLiteral("AppState"), Qt::CaseSensitive);
    if (appState == nullptr) {
        return {};
    }
    const VdfNode *name = findChild(*appState, QStringLiteral("name"),
                                    Qt::CaseSensitive);
    if (name == nullptr || !name->children.isEmpty()) {
        return {};
    }
    if (!isUsableText(name->value, kMaxSteamNameCharacters)) {
        return {};
    }
    return name->value;
}

} // namespace QindaQt::Compositor
