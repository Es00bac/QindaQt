// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QFile>
#include <QString>

#include <memory>

namespace QindaQt::WindowsRunner {
enum class ExecutableFileError {
    None, NotLocalFile, Unreadable, NotPortableExecutable,
    MalformedImage, UnsupportedImage, FileChanged,
};
enum class WindowsImageArchitecture { X86, X86_64, Arm64 };

struct ExecutableFileIdentity final {
    quint64 device = 0;
    quint64 inode = 0;
    quint64 bytes = 0;
    qint64 modifiedSeconds = 0;
    qint64 modifiedNanoseconds = 0;
    qint64 changedSeconds = 0;
    qint64 changedNanoseconds = 0;
    friend bool operator==(const ExecutableFileIdentity &,
                           const ExecutableFileIdentity &) = default;
};

// Owns the exact opened regular file through setup/dispatch. A descriptor is
// borrowed, never closed by a consumer. Setup retains this lease or duplicates
// its handle; reopening its pathname loses the selected-file identity fence.
struct WindowsExecutableInspection;
class WindowsExecutableFile final {
public:
    ~WindowsExecutableFile() = default;
    [[nodiscard]] const QString &canonicalPath() const { return m_canonicalPath; }
    [[nodiscard]] const ExecutableFileIdentity &identity() const { return m_identity; }
    [[nodiscard]] WindowsImageArchitecture architecture() const { return m_architecture; }
    [[nodiscard]] int borrowedDescriptor() const;
    [[nodiscard]] bool stillCurrent() const;
private:
    friend WindowsExecutableInspection inspectWindowsExecutable(const QString &);
    WindowsExecutableFile(std::unique_ptr<QFile> file, QString canonicalPath,
                          ExecutableFileIdentity identity,
                          WindowsImageArchitecture architecture);
    std::unique_ptr<QFile> m_file;
    QString m_canonicalPath;
    ExecutableFileIdentity m_identity;
    WindowsImageArchitecture m_architecture;
};

struct WindowsExecutableInspection final {
    std::unique_ptr<WindowsExecutableFile> file;
    ExecutableFileError error = ExecutableFileError::None;
    QString message;
};

// Opens one absolute local readable regular target, resolves a symlink once,
// and reads at most 8KiB of DOS/PE/COFF/optional/section headers. Bounds file
// offsets before seeking; accepts x86/x64/Arm64 PE executables, not DLLs/DOS
// stubs/scripts or extension-only matches. Not a malware or origin authority.
// Synchronous bounded local I/O; callers keep setup/hash work off a GUI thread.
[[nodiscard]] WindowsExecutableInspection inspectWindowsExecutable(
    const QString &absolutePath);
} // namespace QindaQt::WindowsRunner
