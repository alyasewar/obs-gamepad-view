#include "json.h"

#include <cstdlib>

namespace ivjson {
namespace {

struct Parser {
  const std::string &text;
  size_t pos = 0;
  bool ok = true;

  explicit Parser(const std::string &t) : text(t) {}

  void skip_ws() {
    while (pos < text.size()) {
      char c = text[pos];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        pos++;
      } else {
        break;
      }
    }
  }

  bool eof() const { return pos >= text.size(); }
  char peek() const { return pos < text.size() ? text[pos] : '\0'; }

  bool literal(const char *word) {
    size_t len = 0;
    while (word[len]) len++;
    if (text.compare(pos, len, word) != 0) return false;
    pos += len;
    return true;
  }

  void parse_string(std::string &out) {
    if (peek() != '"') {
      ok = false;
      return;
    }
    pos++;
    while (!eof()) {
      char c = text[pos++];
      if (c == '"') return;
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (eof()) break;
      char esc = text[pos++];
      switch (esc) {
        case 'n': out.push_back('\n'); break;
        case 't': out.push_back('\t'); break;
        case 'r': out.push_back('\r'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'u': {
          // Only the BMP subset that matters here; encode as UTF-8.
          if (pos + 4 > text.size()) { ok = false; return; }
          unsigned code = static_cast<unsigned>(strtoul(text.substr(pos, 4).c_str(), nullptr, 16));
          pos += 4;
          if (code < 0x80) {
            out.push_back(static_cast<char>(code));
          } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          } else {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          }
          break;
        }
        default: out.push_back(esc); break;
      }
    }
    ok = false;
  }

  void parse_value(Value &out) {
    if (!ok) return;
    skip_ws();
    if (eof()) {
      ok = false;
      return;
    }

    char c = peek();
    if (c == '{') {
      pos++;
      out.type = Value::Type::Object;
      skip_ws();
      if (peek() == '}') { pos++; return; }
      while (ok) {
        skip_ws();
        std::string key;
        parse_string(key);
        if (!ok) return;
        skip_ws();
        if (peek() != ':') { ok = false; return; }
        pos++;
        Value child;
        parse_value(child);
        if (!ok) return;
        out.object[key] = child;
        skip_ws();
        if (peek() == ',') { pos++; continue; }
        if (peek() == '}') { pos++; return; }
        ok = false;
        return;
      }
      return;
    }

    if (c == '[') {
      pos++;
      out.type = Value::Type::Array;
      skip_ws();
      if (peek() == ']') { pos++; return; }
      while (ok) {
        Value child;
        parse_value(child);
        if (!ok) return;
        out.array.push_back(child);
        skip_ws();
        if (peek() == ',') { pos++; continue; }
        if (peek() == ']') { pos++; return; }
        ok = false;
        return;
      }
      return;
    }

    if (c == '"') {
      out.type = Value::Type::String;
      parse_string(out.string);
      return;
    }

    if (literal("true")) { out.type = Value::Type::Bool; out.boolean = true; return; }
    if (literal("false")) { out.type = Value::Type::Bool; out.boolean = false; return; }
    if (literal("null")) { out.type = Value::Type::Null; return; }

    if (c == '-' || (c >= '0' && c <= '9')) {
      size_t start = pos;
      if (peek() == '-') pos++;
      while (!eof()) {
        char d = text[pos];
        if ((d >= '0' && d <= '9') || d == '.' || d == 'e' || d == 'E' || d == '+' || d == '-') {
          pos++;
        } else {
          break;
        }
      }
      out.type = Value::Type::Number;
      out.number = strtod(text.substr(start, pos - start).c_str(), nullptr);
      return;
    }

    ok = false;
  }
};

}  // namespace

bool parse(const std::string &text, Value &out) {
  Parser parser(text);
  Value root;
  parser.parse_value(root);
  if (!parser.ok) return false;
  parser.skip_ws();
  if (!parser.eof()) return false;
  out = root;
  return true;
}

}  // namespace ivjson
