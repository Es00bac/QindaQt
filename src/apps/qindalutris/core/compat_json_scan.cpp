// SPDX-License-Identifier: GPL-3.0-or-later
#include "compat_json_scan.h"

#include <QSet>
#include <QString>

namespace QindaQt::QindaLutris::CompatJson {
namespace {

// Strict UTF-8 over the whole buffer, as Python's "utf-8" codec decodes it.
bool isStrictUtf8(const QByteArray &bytes) {
  const auto *p = reinterpret_cast<const unsigned char *>(bytes.constData());
  const auto *end = p + bytes.size();
  while (p < end) {
    const unsigned char lead = *p;
    if (lead < 0x80) {
      ++p;
      continue;
    }
    int length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
      length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      length = 3;
      if (lead == 0xE0) low = 0xA0;  // overlong
      if (lead == 0xED) high = 0x9F; // UTF-16 surrogates
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      length = 4;
      if (lead == 0xF0) low = 0x90;  // overlong
      if (lead == 0xF4) high = 0x8F; // above U+10FFFF
    } else {
      return false;
    }
    if (end - p < length) return false;
    if (p[1] < low || p[1] > high) return false;
    for (int i = 2; i < length; ++i) {
      if (p[i] < 0x80 || p[i] > 0xBF) return false;
    }
    p += length;
  }
  return true;
}

class Scanner {
public:
  explicit Scanner(const QByteArray &bytes)
      : m_p(bytes.constData()), m_end(bytes.constData() + bytes.size()) {}

  bool document() {
    skipSpace();
    if (!value(0)) return false;
    skipSpace();
    return m_p == m_end;
  }

private:
  const char *m_p;
  const char *m_end;

  bool atEnd() const { return m_p >= m_end; }
  char peek() const { return atEnd() ? '\0' : *m_p; }
  static bool isDigit(char ch) { return ch >= '0' && ch <= '9'; }

  void skipSpace() {
    while (!atEnd() && (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r')) ++m_p;
  }

  bool literal(const char *word) {
    for (const char *w = word; *w != '\0'; ++w, ++m_p) {
      if (atEnd() || *m_p != *w) return false;
    }
    return true;
  }

  bool digits() {
    if (atEnd() || !isDigit(*m_p)) return false;
    while (!atEnd() && isDigit(*m_p)) ++m_p;
    return true;
  }

  // -? (0 | [1-9][0-9]*) (. [0-9]+)? ([eE] [+-]? [0-9]+)?
  bool number() {
    if (peek() == '-') ++m_p;
    if (peek() == '0') {
      ++m_p;
    } else if (!digits()) {
      return false;
    }
    if (peek() == '.') {
      ++m_p;
      if (!digits()) return false;
    }
    if (peek() == 'e' || peek() == 'E') {
      ++m_p;
      if (peek() == '+' || peek() == '-') ++m_p;
      if (!digits()) return false;
    }
    return true;
  }

  static int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  }

  // A string starting at '"'. When `decoded` is set the unescaped text is
  // collected as UTF-16 (lone surrogate escapes kept as single units, as
  // Python keeps them as single code points).
  bool string(QString *decoded) {
    ++m_p; // opening quote
    const char *run = m_p;
    const auto flush = [&] {
      if (decoded != nullptr && m_p > run) {
        decoded->append(QString::fromUtf8(run, qsizetype(m_p - run)));
      }
    };
    while (true) {
      if (atEnd()) return false;
      const auto ch = static_cast<unsigned char>(*m_p);
      if (ch == '"') {
        flush();
        ++m_p;
        return true;
      }
      if (ch < 0x20) return false;
      if (ch != '\\') {
        ++m_p;
        continue;
      }
      flush();
      ++m_p;
      if (atEnd()) return false;
      const char escape = *m_p++;
      char16_t unit = 0;
      switch (escape) {
      case '"': unit = u'"'; break;
      case '\\': unit = u'\\'; break;
      case '/': unit = u'/'; break;
      case 'b': unit = u'\b'; break;
      case 'f': unit = u'\f'; break;
      case 'n': unit = u'\n'; break;
      case 'r': unit = u'\r'; break;
      case 't': unit = u'\t'; break;
      case 'u': {
        if (m_end - m_p < 4) return false;
        int code = 0;
        for (int i = 0; i < 4; ++i) {
          const int digit = hexValue(m_p[i]);
          if (digit < 0) return false;
          code = code * 16 + digit;
        }
        m_p += 4;
        unit = char16_t(code);
        break;
      }
      default:
        return false;
      }
      if (decoded != nullptr) decoded->append(QChar(unit));
      run = m_p;
    }
  }

  bool object(int depth) {
    ++m_p; // '{'
    QSet<QString> keys;
    skipSpace();
    if (peek() == '}') {
      ++m_p;
      return true;
    }
    while (true) {
      skipSpace();
      if (peek() != '"') return false;
      QString key;
      if (!string(&key) || keys.contains(key)) return false;
      keys.insert(key);
      skipSpace();
      if (peek() != ':') return false;
      ++m_p;
      skipSpace();
      if (!value(depth + 1)) return false;
      skipSpace();
      if (peek() == ',') {
        ++m_p;
        continue;
      }
      if (peek() != '}') return false;
      ++m_p;
      return true;
    }
  }

  bool array(int depth) {
    ++m_p; // '['
    skipSpace();
    if (peek() == ']') {
      ++m_p;
      return true;
    }
    while (true) {
      skipSpace();
      if (!value(depth + 1)) return false;
      skipSpace();
      if (peek() == ',') {
        ++m_p;
        continue;
      }
      if (peek() != ']') return false;
      ++m_p;
      return true;
    }
  }

  bool value(int depth) {
    if (depth >= kMaxDepth || atEnd()) return false;
    switch (*m_p) {
    case '{': return object(depth);
    case '[': return array(depth);
    case '"': return string(nullptr);
    case 't': return literal("true");
    case 'f': return literal("false");
    case 'n': return literal("null");
    default: return number();
    }
  }
};

} // namespace

bool isStrictJson(const QByteArray &bytes) {
  // A BOM is not JSON whitespace, so the grammar below refuses it anyway;
  // the explicit check keeps the reason obvious.
  if (bytes.startsWith("\xEF\xBB\xBF") || !isStrictUtf8(bytes)) return false;
  return Scanner(bytes).document();
}

} // namespace QindaQt::QindaLutris::CompatJson
