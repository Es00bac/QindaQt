// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/windows_runner/executable_file.h>

#include <QDir>
#include <QFileInfo>
#include <QtEndian>

#include <sys/stat.h>

namespace QindaQt::WindowsRunner {
namespace {
constexpr qint64 maximumHeaderOffset = 1024 * 1024;

ExecutableFileIdentity fileIdentity(const struct stat &attributes)
{
    return {static_cast<quint64>(attributes.st_dev), static_cast<quint64>(attributes.st_ino),
            static_cast<quint64>(attributes.st_size),
            static_cast<qint64>(attributes.st_mtim.tv_sec), static_cast<qint64>(attributes.st_mtim.tv_nsec),
            static_cast<qint64>(attributes.st_ctim.tv_sec), static_cast<qint64>(attributes.st_ctim.tv_nsec)};
}

WindowsExecutableInspection failed(ExecutableFileError error)
{
    QString message;
    switch (error) {
    case ExecutableFileError::NotLocalFile:
        message = QStringLiteral("Choose a local regular Windows program file."); break;
    case ExecutableFileError::Unreadable:
        message = QStringLiteral("This Windows program file cannot be read."); break;
    case ExecutableFileError::NotPortableExecutable:
        message = QStringLiteral("This file is not a Windows PE executable."); break;
    case ExecutableFileError::UnsupportedImage:
        message = QStringLiteral("This Windows image is not a supported desktop executable."); break;
    case ExecutableFileError::FileChanged:
        message = QStringLiteral("The selected Windows program changed. Select it again."); break;
    default:
        message = QStringLiteral("This Windows executable has invalid or incomplete headers."); break;
    }
    return {{}, error, message};
}

quint16 u16(const QByteArray &bytes, qsizetype offset)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}
quint32 u32(const QByteArray &bytes, qsizetype offset)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}
} // namespace

WindowsExecutableFile::WindowsExecutableFile(std::unique_ptr<QFile> file,
    QString canonicalPath, ExecutableFileIdentity selectedIdentity,
    WindowsImageArchitecture architecture)
    : m_file(std::move(file)), m_canonicalPath(std::move(canonicalPath)),
      m_identity(selectedIdentity), m_architecture(architecture)
{
}

int WindowsExecutableFile::borrowedDescriptor() const
{
    return m_file ? m_file->handle() : -1;
}

bool WindowsExecutableFile::stillCurrent() const
{
    struct stat held{};
    struct stat named{};
    const QByteArray path = QFile::encodeName(m_canonicalPath);
    return m_file && ::fstat(borrowedDescriptor(), &held) == 0
        && ::lstat(path.constData(), &named) == 0 && S_ISREG(named.st_mode)
        && held.st_size >= 0 && named.st_size >= 0
        && fileIdentity(held) == m_identity && fileIdentity(named) == m_identity;
}

WindowsExecutableInspection inspectWindowsExecutable(const QString &absolutePath)
{
    if (!QDir::isAbsolutePath(absolutePath) || absolutePath.contains(QChar(0))) {
        return failed(ExecutableFileError::NotLocalFile);
    }
    const QFileInfo requested(absolutePath);
    const QString canonical = requested.canonicalFilePath();
    if (canonical.isEmpty() || !QFileInfo(canonical).isFile()) {
        return failed(ExecutableFileError::NotLocalFile);
    }
    auto file = std::make_unique<QFile>(canonical);
    if (!file->open(QIODevice::ReadOnly)) {
        return failed(ExecutableFileError::Unreadable);
    }
    struct stat attributes{};
    if (::fstat(file->handle(), &attributes) != 0 || !S_ISREG(attributes.st_mode) || attributes.st_size < 64) {
        return failed(ExecutableFileError::NotPortableExecutable);
    }
    const auto originalIdentity = fileIdentity(attributes);
    const QByteArray dos = file->read(64);
    if (dos.size() != 64 || dos.left(2) != QByteArrayLiteral("MZ")) {
        return failed(ExecutableFileError::NotPortableExecutable);
    }
    const qint64 peOffset = u32(dos, 60);
    if (peOffset < 64 || peOffset > maximumHeaderOffset || peOffset > file->size() - 24
        || !file->seek(peOffset)) {
        return failed(ExecutableFileError::MalformedImage);
    }
    const QByteArray coff = file->read(24);
    if (coff.size() != 24 || coff.left(4) != QByteArray("PE\0\0", 4)) {
        return failed(ExecutableFileError::NotPortableExecutable);
    }
    const quint16 machine = u16(coff, 4);
    const quint16 sections = u16(coff, 6);
    const quint16 optionalSize = u16(coff, 20);
    const quint16 characteristics = u16(coff, 22);
    if ((characteristics & 0x0002U) == 0 || (characteristics & 0x2000U) != 0) {
        return failed(ExecutableFileError::UnsupportedImage);
    }
    WindowsImageArchitecture architecture;
    switch (machine) {
    case 0x014c: architecture = WindowsImageArchitecture::X86; break;
    case 0x8664: architecture = WindowsImageArchitecture::X86_64; break;
    case 0xaa64: architecture = WindowsImageArchitecture::Arm64; break;
    default: return failed(ExecutableFileError::UnsupportedImage);
    }
    if (sections == 0 || sections > 96 || optionalSize < 96 || optionalSize > 4096
        || peOffset + 24 + optionalSize + static_cast<qint64>(sections) * 40 > file->size()) {
        return failed(ExecutableFileError::MalformedImage);
    }
    const QByteArray optional = file->read(optionalSize);
    if (optional.size() != optionalSize
        || (machine == 0x014c && u16(optional, 0) != 0x010b)
        || (machine != 0x014c && (u16(optional, 0) != 0x020b || optionalSize < 112))) {
        return failed(ExecutableFileError::MalformedImage);
    }
    const quint16 subsystem = u16(optional, 68);
    if (subsystem != 2 && subsystem != 3) {
        return failed(ExecutableFileError::UnsupportedImage);
    }
    const quint32 entrypoint = u32(optional, 16);
    bool executableEntrypoint = false;
    const QByteArray table = file->read(static_cast<qint64>(sections) * 40);
    if (table.size() != static_cast<qint64>(sections) * 40) {
        return failed(ExecutableFileError::MalformedImage);
    }
    for (quint16 section = 0; section < sections; ++section) {
        const qsizetype offset = static_cast<qsizetype>(section) * 40;
        const quint64 virtualSize = u32(table, offset + 8);
        const quint64 address = u32(table, offset + 12);
        const quint64 rawSize = u32(table, offset + 16);
        const quint64 rawOffset = u32(table, offset + 20);
        if (rawSize > 0 && (rawOffset < static_cast<quint64>(peOffset + 24 + optionalSize
                + static_cast<qint64>(sections) * 40)
            || rawOffset > originalIdentity.bytes || rawSize > originalIdentity.bytes - rawOffset)) {
            return failed(ExecutableFileError::MalformedImage);
        }
        if ((u32(table, offset + 36) & 0x20000000U) != 0
            && entrypoint >= address && entrypoint - address < qMax(virtualSize, rawSize)) {
            executableEntrypoint = true;
        }
    }
    if (!executableEntrypoint) {
        return failed(ExecutableFileError::MalformedImage);
    }
    auto opened = std::unique_ptr<WindowsExecutableFile>(new WindowsExecutableFile(
        std::move(file), canonical, originalIdentity, architecture));
    if (!opened->stillCurrent()) {
        return failed(ExecutableFileError::FileChanged);
    }
    return {std::move(opened), ExecutableFileError::None, {}};
}
} // namespace QindaQt::WindowsRunner
