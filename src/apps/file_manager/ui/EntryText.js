// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

// What the four views (ADR-0270) say about an entry, in one place so Icons,
// Details, Columns and Gallery never disagree. Pure functions over the
// NavigationController entry maps; an empty string means "unknown" and a
// view shows a dash for it, never a zero.

// QLocale::ShortFormat, which Date.toLocale*String() accepts as a number.
var shortFormat = 1

function validDate(value) {
    return value instanceof Date && !isNaN(value.getTime())
}

// The locale's short date and time, or "Today, 14:05" / "Yesterday, 09:12"
// when relative dates are on and the day allows it.
function dateText(value, relative, locale) {
    if (!validDate(value))
        return ""
    if (relative) {
        const now = new Date()
        const today = new Date(now.getFullYear(), now.getMonth(), now.getDate())
        const day = new Date(value.getFullYear(), value.getMonth(), value.getDate())
        const days = Math.round((today.getTime() - day.getTime()) / 86400000)
        const time = value.toLocaleTimeString(locale, shortFormat)
        if (days === 0)
            return qsTr("Today, %1").arg(time)
        if (days === 1)
            return qsTr("Yesterday, %1").arg(time)
    }
    return value.toLocaleString(locale, shortFormat)
}

// Text after the last dot of a file's name; folders, applications and dot
// files ("bashrc" is a name, not an extension) have none. listing_order.cpp's
// Extension sort uses the same rule.
function extensionOf(entry) {
    if (!entry || entry.isDirectory || entry.applicationId)
        return ""
    const name = String(entry.name || "")
    const dot = name.lastIndexOf(".")
    return dot > 0 && dot < name.length - 1 ? name.substring(dot + 1) : ""
}

// The name a view shows: without its extension when the user hides
// extensions. Rename and every file operation still use the whole name.
function displayName(entry, showExtensions) {
    const name = String(entry && entry.name !== undefined ? entry.name : "")
    if (showExtensions)
        return name
    const extension = extensionOf(entry)
    return extension.length > 0 ? name.substring(0, name.length - extension.length - 1) : name
}

// "drwxr-xr-x" from the listing's decimal mode; "" when the listing has no
// mode (a network or Applications row).
function permissionsText(entry) {
    const mode = Number(entry && entry.mode ? entry.mode : 0)
    if (!(mode > 0))
        return ""
    const type = mode & 0o170000
    let text = type === 0o040000 ? "d" : type === 0o120000 ? "l" : "-"
    const letters = "rwxrwxrwx"
    for (let bit = 0; bit < 9; ++bit)
        text += (mode & (0o400 >> bit)) !== 0 ? letters[bit] : "-"
    return text
}

// The folder holding an entry, for search results' Path column.
function folderOf(entry) {
    const path = String(entry && entry.path ? entry.path : "")
    const slash = path.lastIndexOf("/")
    return slash > 0 ? path.substring(0, slash) : slash === 0 ? "/" : ""
}

// ", folder" / ", file" / ", application": what an accessible name adds.
function kindSuffix(entry) {
    if (entry && entry.applicationId)
        return qsTr(", application")
    return entry && entry.isDirectory ? qsTr(", folder") : qsTr(", file")
}

// A QindaTK glyph (Tk.Icon) that stands for an entry while its picture loads.
function glyphFor(entry) {
    const icon = String(entry && entry.iconName ? entry.iconName : "")
    if (entry && entry.isDirectory)
        return "folder"
    if (entry && entry.applicationId)
        return "package"
    if (icon === "image-x-generic")
        return "image"
    if (icon === "audio-x-generic")
        return "music"
    if (icon === "video-x-generic")
        return "film"
    if (icon === "application-x-archive")
        return "archive"
    if (icon === "application-pdf" || icon.indexOf("text") >= 0)
        return "file-text"
    return "file"
}

// The theme icon an entry is drawn with (the image://theme-icons provider),
// rendered `size` pixels square: the provider's size hint stands in for
// Image.sourceSize, which a Tk.Thumbnail does not expose.
function iconUrl(entry, size) {
    return "image://theme-icons/" + (entry && entry.iconName ? entry.iconName
                                                              : "application-octet-stream")
        + "?size=" + Math.max(16, Math.round(size))
}

// AGENT-CONTRACT: main.cpp registers the Gallery's larger preview pipeline as
// "gallery-previews" beside "previews"; the id after the provider name is the
// same, so the larger picture is the same file, identity and generation.
function galleryPreviewUrl(entry) {
    const url = String(entry && entry.previewUrl ? entry.previewUrl : "")
    return url.length > 0 ? url.replace("image://previews/", "image://gallery-previews/") : ""
}
