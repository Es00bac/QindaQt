// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QMetaType>
#include <QString>
namespace QindaQt::Viewer {
struct PageText {
    bool available = false;
    QString text;
    QString error;
};
enum class TextSearchStatus { Found, NotFound, Cancelled, Unavailable, Limit };
struct TextSearchRequest {
    quint64 revision = 0;
    QString path;
    QByteArray password;
    QString query;
    int page = 0;
    int offset = 0;
    bool backward = false;
    bool caseSensitive = false;
};
struct TextSearchResult {
    quint64 revision = 0;
    TextSearchStatus status = TextSearchStatus::Cancelled;
    QString error;
    int page = -1;
    int start = -1;
    int length = 0;
    QString text;
};
inline constexpr qsizetype MaxPageText = 262144;
inline constexpr qsizetype MaxSearchQuery = 512;
inline constexpr int MaxSearchPages = 4096;
}
Q_DECLARE_METATYPE(QindaQt::Viewer::TextSearchResult)
