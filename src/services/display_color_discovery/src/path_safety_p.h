// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>
#include <utility>

namespace QindaQt::DisplayColor
{

class CanonicalRootContainment final
{
public:
    static std::optional<CanonicalRootContainment> resolve(const QString &injectedRoot)
    {
        if (injectedRoot.isEmpty() || injectedRoot.contains(QChar(u'\0')) ||
            hasParentReference(injectedRoot)) {
            return std::nullopt;
        }

        const QFileInfo rootInfo(injectedRoot);
        const QString lexicalRoot = QDir::cleanPath(rootInfo.absoluteFilePath());
        const QString canonicalRoot = rootInfo.canonicalFilePath();
        // AGENT-GUARD: Equality rejects a symlink in any root component. The
        // canonical path is still retained as the sole containment authority
        // for every later candidate; do not fall back to ancestor walking,
        // which can erase a hostile ".." before seeing the redirect.
        if (canonicalRoot.isEmpty() || canonicalRoot != lexicalRoot) {
            return std::nullopt;
        }
        return CanonicalRootContainment(canonicalRoot);
    }

    [[nodiscard]] const QString &canonicalRoot() const noexcept { return m_canonicalRoot; }

    [[nodiscard]] bool containsExistingPath(const QString &candidate) const
    {
        if (candidate.isEmpty() || hasParentReference(candidate)) {
            return false;
        }
        return containsCanonicalPath(QFileInfo(candidate).canonicalFilePath(), false);
    }

    [[nodiscard]] bool containsDestinationPath(const QString &candidate) const
    {
        if (candidate.isEmpty() || hasParentReference(candidate)) {
            return false;
        }
        const QFileInfo candidateInfo(candidate);
        const QString canonicalCandidate = candidateInfo.canonicalFilePath();
        if (!canonicalCandidate.isEmpty()) {
            return containsCanonicalPath(canonicalCandidate, false);
        }

        // A destination may not exist yet. Its existing parent is therefore
        // the authority boundary; failure to canonicalize that parent is a
        // closed rejection, never permission to probe the child.
        const QString canonicalParent = QFileInfo(candidateInfo.absolutePath()).canonicalFilePath();
        return containsCanonicalPath(canonicalParent, true);
    }

private:
    explicit CanonicalRootContainment(QString canonicalRoot)
        : m_canonicalRoot(std::move(canonicalRoot))
    {
    }

    static bool hasParentReference(const QString &path)
    {
        const QString portable = QDir::fromNativeSeparators(path);
        const QStringList components = portable.split(QChar(u'/'), Qt::SkipEmptyParts);
        return components.contains(QStringLiteral(".."));
    }

    [[nodiscard]] bool containsCanonicalPath(const QString &candidate,
                                             bool rootItselfAllowed) const
    {
        if (candidate.isEmpty()) {
            return false;
        }
        if (candidate == m_canonicalRoot) {
            return rootItselfAllowed;
        }
        QString prefix = m_canonicalRoot;
        if (!prefix.endsWith(QChar(u'/'))) {
            prefix.append(QChar(u'/'));
        }
        return candidate.startsWith(prefix);
    }

    QString m_canonicalRoot;
};

} // namespace QindaQt::DisplayColor
