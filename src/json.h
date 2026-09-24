#pragma once

// Minimal recursive-descent JSON reader.
//
// The layout manifests nest objects and arrays ("src": [668, 314, 64, 64]),
// which the previous substring-scanning helper could not represent. This is
// deliberately small rather than a vendored library: it parses the subset JSON
// actually uses -- objects, arrays, strings, numbers, bools, null -- and never
// throws, so a malformed theme degrades to defaults instead of taking OBS down.

#include <map>
#include <string>
#include <vector>

namespace ivjson {

class Value {
public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Type type = Type::Null;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Value> array;
  std::map<std::string, Value> object;

  bool is_null() const { return type == Type::Null; }

  // Missing keys yield a Null value, so chained lookups stay safe.
  const Value &operator[](const std::string &key) const {
    static const Value null_value;
    if (type != Type::Object) return null_value;
    auto it = object.find(key);
    return it == object.end() ? null_value : it->second;
  }

  const Value &operator[](size_t index) const {
    static const Value null_value;
    if (type != Type::Array || index >= array.size()) return null_value;
    return array[index];
  }

  size_t size() const { return type == Type::Array ? array.size() : 0; }

  std::string str(const std::string &fallback = "") const {
    return type == Type::String ? string : fallback;
  }

  double num(double fallback = 0.0) const {
    return type == Type::Number ? number : fallback;
  }

  int integer(int fallback = 0) const {
    return type == Type::Number ? static_cast<int>(number) : fallback;
  }

  bool flag(bool fallback = false) const {
    return type == Type::Bool ? boolean : fallback;
  }
};

// Returns false and leaves `out` untouched if the text is not valid JSON.
bool parse(const std::string &text, Value &out);

}  // namespace ivjson
