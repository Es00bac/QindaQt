// SPDX-License-Identifier: LGPL-3.0-or-later
#include "profile_json_syntax_p.h"

#include "profile_path_p.h"

#include <QSet>
#include <QStringDecoder>

#include <utility>

namespace QindaQt::Profiles::Internal {
namespace {

constexpr int maximumJsonNesting = 64;

class StrictJsonSyntax final {
public:
    StrictJsonSyntax(const QByteArray &json, QString origin)
        : m_json(json)
        , m_origin(std::move(origin))
    {
    }

    [[nodiscard]] ProfileError validate()
    {
        skipWhitespace();
        if (!parseValue({}, 0)) {
            return m_error;
        }
        skipWhitespace();
        if (m_position != m_json.size()) {
            fail(ProfileErrorCode::InvalidJson,
                 {},
                 QStringLiteral("unexpected content after the root value"));
        }
        return m_error;
    }

private:
    [[nodiscard]] bool parseValue(const QString &path, int depth)
    {
        if (depth > maximumJsonNesting) {
            return fail(ProfileErrorCode::ExcessiveNesting,
                        path,
                        QStringLiteral("JSON nesting exceeds the limit of %1")
                            .arg(maximumJsonNesting));
        }
        if (m_position >= m_json.size()) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("expected a JSON value"));
        }

        switch (m_json.at(m_position)) {
        case '{':
            return parseObject(path, depth);
        case '[':
            return parseArray(path, depth);
        case '"': {
            QString ignored;
            return parseString(path, &ignored);
        }
        case 't':
            return parseLiteral(QByteArrayLiteral("true"), path);
        case 'f':
            return parseLiteral(QByteArrayLiteral("false"), path);
        case 'n':
            return parseLiteral(QByteArrayLiteral("null"), path);
        default:
            if (m_json.at(m_position) == '-' || isDigit(m_json.at(m_position))) {
                return parseNumber(path);
            }
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("unexpected byte while reading a JSON value"));
        }
    }

    [[nodiscard]] bool parseObject(const QString &path, int depth)
    {
        ++m_position;
        skipWhitespace();
        if (consume('}')) {
            return true;
        }

        QSet<QString> keys;
        while (true) {
            const qsizetype keyOffset = m_position;
            QString key;
            if (!parseString(path, &key)) {
                return false;
            }
            const QString childPath = jsonPointerChild(path, key);
            if (keys.contains(key)) {
                return fail(ProfileErrorCode::DuplicateJsonKey,
                            childPath,
                            QStringLiteral("object repeats the key '%1'").arg(key),
                            keyOffset);
            }
            keys.insert(key);
            skipWhitespace();
            if (!consume(':')) {
                return fail(ProfileErrorCode::InvalidJson,
                            childPath,
                            QStringLiteral("expected ':' after an object key"));
            }
            skipWhitespace();
            if (!parseValue(childPath, depth + 1)) {
                return false;
            }
            skipWhitespace();
            if (consume('}')) {
                return true;
            }
            if (!consume(',')) {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("expected ',' or '}' in an object"));
            }
            skipWhitespace();
        }
    }

    [[nodiscard]] bool parseArray(const QString &path, int depth)
    {
        ++m_position;
        skipWhitespace();
        if (consume(']')) {
            return true;
        }

        qsizetype index = 0;
        while (true) {
            if (!parseValue(jsonPointerIndex(path, index), depth + 1)) {
                return false;
            }
            ++index;
            skipWhitespace();
            if (consume(']')) {
                return true;
            }
            if (!consume(',')) {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("expected ',' or ']' in an array"));
            }
            skipWhitespace();
        }
    }

    [[nodiscard]] bool parseString(const QString &path, QString *decoded)
    {
        if (!consume('"')) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("expected a JSON string"));
        }

        while (m_position < m_json.size()) {
            const qsizetype segmentStart = m_position;
            while (m_position < m_json.size()) {
                const auto byte = static_cast<unsigned char>(m_json.at(m_position));
                if (byte == '"' || byte == '\\' || byte < 0x20U) {
                    break;
                }
                ++m_position;
            }
            if (!appendUtf8(segmentStart, m_position, path, decoded)) {
                return false;
            }
            if (m_position >= m_json.size()) {
                break;
            }

            const auto byte = static_cast<unsigned char>(m_json.at(m_position));
            if (byte == '"') {
                ++m_position;
                return true;
            }
            if (byte < 0x20U) {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("unescaped control byte in a JSON string"));
            }

            const qsizetype escapeOffset = m_position;
            ++m_position;
            if (m_position >= m_json.size()) {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("unterminated escape sequence"),
                            escapeOffset);
            }
            const char escape = m_json.at(m_position++);
            switch (escape) {
            case '"':
                decoded->append(QLatin1Char('"'));
                break;
            case '\\':
                decoded->append(QLatin1Char('\\'));
                break;
            case '/':
                decoded->append(QLatin1Char('/'));
                break;
            case 'b':
                decoded->append(QLatin1Char('\b'));
                break;
            case 'f':
                decoded->append(QLatin1Char('\f'));
                break;
            case 'n':
                decoded->append(QLatin1Char('\n'));
                break;
            case 'r':
                decoded->append(QLatin1Char('\r'));
                break;
            case 't':
                decoded->append(QLatin1Char('\t'));
                break;
            case 'u':
                if (!parseEscapedCodePoint(path, escapeOffset, decoded)) {
                    return false;
                }
                break;
            default:
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("unknown JSON escape sequence"),
                            escapeOffset);
            }
        }
        return fail(ProfileErrorCode::InvalidJson,
                    path,
                    QStringLiteral("unterminated JSON string"));
    }

    [[nodiscard]] bool parseEscapedCodePoint(const QString &path,
                                             qsizetype escapeOffset,
                                             QString *decoded)
    {
        char32_t first = 0;
        if (!readHexCodeUnit(&first)) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("Unicode escape requires four hexadecimal digits"),
                        escapeOffset);
        }
        if (first >= 0xDC00U && first <= 0xDFFFU) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("lone low surrogate in a Unicode escape"),
                        escapeOffset);
        }
        if (first < 0xD800U || first > 0xDBFFU) {
            decoded->append(QChar(static_cast<char16_t>(first)));
            return true;
        }

        if (m_json.size() - m_position < 2 || m_json.at(m_position) != '\\'
            || m_json.at(m_position + 1) != 'u') {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("high surrogate requires a following low surrogate"),
                        escapeOffset);
        }
        m_position += 2;
        char32_t second = 0;
        if (!readHexCodeUnit(&second) || second < 0xDC00U || second > 0xDFFFU) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("high surrogate requires a following low surrogate"),
                        escapeOffset);
        }
        const char32_t codePoint =
            0x10000U + ((first - 0xD800U) << 10U) + (second - 0xDC00U);
        decoded->append(QString::fromUcs4(&codePoint, 1));
        return true;
    }

    [[nodiscard]] bool readHexCodeUnit(char32_t *value)
    {
        if (m_json.size() - m_position < 4) {
            return false;
        }
        char32_t result = 0;
        for (int index = 0; index < 4; ++index) {
            const int digit = hexDigit(m_json.at(m_position + index));
            if (digit < 0) {
                return false;
            }
            result = (result << 4U) | static_cast<char32_t>(digit);
        }
        m_position += 4;
        *value = result;
        return true;
    }

    [[nodiscard]] bool appendUtf8(qsizetype begin,
                                  qsizetype end,
                                  const QString &path,
                                  QString *decoded)
    {
        if (begin == end) {
            return true;
        }
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString text = decoder(QByteArrayView(m_json).sliced(begin, end - begin));
        if (decoder.hasError()) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("invalid UTF-8 in a JSON string"),
                        begin);
        }
        decoded->append(text);
        return true;
    }

    [[nodiscard]] bool parseNumber(const QString &path)
    {
        if (consume('-') && m_position >= m_json.size()) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("incomplete JSON number"));
        }
        if (consume('0')) {
            if (m_position < m_json.size() && isDigit(m_json.at(m_position))) {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("JSON number contains a leading zero"));
            }
        } else {
            if (m_position >= m_json.size() || m_json.at(m_position) < '1'
                || m_json.at(m_position) > '9') {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("invalid JSON number"));
            }
            while (m_position < m_json.size() && isDigit(m_json.at(m_position))) {
                ++m_position;
            }
        }
        if (consume('.')) {
            if (m_position >= m_json.size() || !isDigit(m_json.at(m_position))) {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("fraction requires at least one digit"));
            }
            while (m_position < m_json.size() && isDigit(m_json.at(m_position))) {
                ++m_position;
            }
        }
        if (m_position < m_json.size()
            && (m_json.at(m_position) == 'e' || m_json.at(m_position) == 'E')) {
            ++m_position;
            if (m_position < m_json.size()
                && (m_json.at(m_position) == '+' || m_json.at(m_position) == '-')) {
                ++m_position;
            }
            if (m_position >= m_json.size() || !isDigit(m_json.at(m_position))) {
                return fail(ProfileErrorCode::InvalidJson,
                            path,
                            QStringLiteral("exponent requires at least one digit"));
            }
            while (m_position < m_json.size() && isDigit(m_json.at(m_position))) {
                ++m_position;
            }
        }
        return true;
    }

    [[nodiscard]] bool parseLiteral(const QByteArray &literal, const QString &path)
    {
        if (literal.size() > m_json.size() - m_position
            || m_json.sliced(m_position, literal.size()) != literal) {
            return fail(ProfileErrorCode::InvalidJson,
                        path,
                        QStringLiteral("invalid JSON literal"));
        }
        m_position += literal.size();
        return true;
    }

    void skipWhitespace()
    {
        while (m_position < m_json.size()) {
            const char byte = m_json.at(m_position);
            if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') {
                return;
            }
            ++m_position;
        }
    }

    [[nodiscard]] bool consume(char expected)
    {
        if (m_position >= m_json.size() || m_json.at(m_position) != expected) {
            return false;
        }
        ++m_position;
        return true;
    }

    bool fail(ProfileErrorCode code,
              QString path,
              QString message,
              qsizetype offset = -1)
    {
        if (!m_error.hasError()) {
            m_error = {.code = code,
                       .origin = m_origin,
                       .path = std::move(path),
                       .panelId = {},
                       .appletId = {},
                       .message = std::move(message),
                       .byteOffset = offset >= 0 ? offset : m_position};
        }
        return false;
    }

    [[nodiscard]] static bool isDigit(char byte)
    {
        return byte >= '0' && byte <= '9';
    }

    [[nodiscard]] static int hexDigit(char byte)
    {
        if (byte >= '0' && byte <= '9') {
            return byte - '0';
        }
        if (byte >= 'a' && byte <= 'f') {
            return byte - 'a' + 10;
        }
        if (byte >= 'A' && byte <= 'F') {
            return byte - 'A' + 10;
        }
        return -1;
    }

    const QByteArray &m_json;
    QString m_origin;
    qsizetype m_position = 0;
    ProfileError m_error;
};

} // namespace

ProfileError validateJsonSyntax(const QByteArray &json, const QString &origin)
{
    return StrictJsonSyntax(json, origin).validate();
}

} // namespace QindaQt::Profiles::Internal
