#ifndef JSONS_READER_H_
#define JSONS_READER_H_

#include "Tokenizer.h"
#include <toolbox/Maybe.h>
#include <toolbox/Decimal.h>

namespace jsons {

static size_t jsonEscapeHandler(const char** source, char** destination) {
  ++(*source);
  switch (**source) {
    case '"':
      **destination = '"';
      return 1;
    case '\\':
      **destination = '\\';
      return 1;
    case '/':
      **destination = '/';
      return 1;
    case 'b':
      **destination = '\b';
      return 1;
    case 'f':
      **destination = '\f';
      return 1;
    case 'n':
      **destination = '\n';
      return 1;
    case 'r':
      **destination = '\r';
      return 1;
    case 't':
      **destination = '\t';
      return 1;
    case 'u':
      // TODO implement unicode character handling (\uxxxx)?
    default:
      **destination = '\\';
      ++(*destination);
      **destination = **source;
      return 0;
  }
}

enum struct ValueType { Invalid, Null, Boolean, Integer, Decimal, String, List, Object };

class List;
class Object;

class Value {
public:
  using Tokenizer = IStoringTokenizer;

protected:
  Tokenizer* _tokenizer;
  ValueType _type;
  struct {
    bool boolean;
    toolbox::Decimal decimal;
  } _primitives;

public:
  Value() : _tokenizer(nullptr), _type(ValueType::Invalid), _primitives() {}
  Value(Tokenizer& tokenizer) : _tokenizer(&tokenizer), _type(ValueType::Invalid), _primitives() {}
  Value(Value&& other) : Value() {
    std::swap(_tokenizer, other._tokenizer);
    std::swap(_type, other._type);
    std::swap(_primitives, other._primitives);
  }
  Value& operator=(Value&& other) {
    if (this != &other) {
      std::swap(_tokenizer, other._tokenizer);
      std::swap(_type, other._type);
      std::swap(_primitives, other._primitives);
    }
    return *this;
  }
  Value(const Value& other) = delete;
  Value& operator=(const Value& other) = delete;

  void invalidate() { _type = ValueType::Invalid; }
  bool valid() const { return _type != ValueType::Invalid; }
  ValueType type() const { return _type; }

  void parse() {
    invalidate();
    if (_tokenizer) {
      _tokenizer->skip(); // skip whitespace
      switch (_tokenizer->peek("ntf\"-0123456789[{")) {
        case 'n':
          _tokenizer->nextUntil(" \r\n\t,]}");
          if (strcmp(_tokenizer->current(), "null") == 0) {
            _tokenizer->pop();
            _type = ValueType::Null;
          } else {
            invalidate();
          }
          break;
        case 't':
          _tokenizer->nextUntil(" \r\n\t,]}");
          if (strcmp(_tokenizer->current(), "true") == 0) {
            _primitives.boolean = true;
            _tokenizer->pop();
            _type = ValueType::Boolean;
          } else {
            invalidate();
          }
          break;
        case 'f':
          _tokenizer->nextUntil(" \r\n\t,]}");
          if (strcmp(_tokenizer->current(), "false") == 0) {
            _primitives.boolean = false;
            _tokenizer->pop();
            _type = ValueType::Boolean;
          } else {
            invalidate();
          }
          break;
        case '"':
          _tokenizer->pop(); // remove leading "
          if (_tokenizer->nextUntil("\"", '\\') == '"') {
            _tokenizer->handleEscapedChars('\\', &jsonEscapeHandler);
            _tokenizer->storeToken(0);
            _tokenizer->pop(); // remove string contents
            _tokenizer->pop(); // remove trailing "
            _type = ValueType::String;
          } else {
            // TODO implement "long string" handling?
            // This can be handled similar to a list where the string fragments can be iterated over.
            // and the caller has to decide how to process it (concatenate into one big string or write it somewhere else).
            // Note: escape characters may be split accross fragments, so this must be carried over.
            invalidate();
          }
          break;
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          {
            _tokenizer->nextWhile("-0123456789.");
            auto decimal = toolbox::Decimal::fromString(_tokenizer->current());
            if (decimal.hasValue()) {
              _primitives.decimal = decimal.value();
              if (_primitives.decimal.isInteger()) {
                _type = ValueType::Integer;
              } else {
                _type = ValueType::Decimal;
              }
            } else {
              invalidate();
            }
          }
          break;
        case '[':
          _type = ValueType::List;
          break;
        case '{':
          _type = ValueType::Object;
          break;
        default:
          invalidate();
          break;
      }

      if (valid()) {
        // skip whitespace in advance to make final value consume the input fully
        _tokenizer->skip();
      } else {
        _tokenizer->abort();
      }
    }
  }

  toolbox::Maybe<int32_t> asInteger() const {
    if (_type != ValueType::Integer && _type != ValueType::Decimal) {
      return {};
    }
    return {_primitives.decimal.integer()};
  }

  toolbox::Maybe<toolbox::Decimal> asDecimal() const {
    if (_type != ValueType::Integer && _type != ValueType::Decimal) {
      return {};
    }
    return {_primitives.decimal};
  }

  toolbox::Maybe<bool> asBoolean() const {
    if (_type != ValueType::Boolean) {
      return {};
    }
    return {_primitives.boolean};
  }

  toolbox::Maybe<toolbox::ConstStr> asString() const {
    if (_type != ValueType::String) {
      return {};
    }
    return {_tokenizer->storedToken(0)};
  }

  List asList() const;
  
  Object asObject() const;

  void skip() const;
};

class List final : public Value {
public:
  class Iterator;
  struct EndIterator final {
    bool operator!=(const Iterator& it) const {
      return it != *this;
    }
  };

  class Iterator final {
    Tokenizer* _tokenizer;
    Value _current;

  public:
    Iterator() : _tokenizer(nullptr), _current() {}
    Iterator(Tokenizer& tokenizer) : _tokenizer(&tokenizer), _current(tokenizer) {
      _tokenizer->skip(); // skip whitespace
      if (_tokenizer->peek("]") == ']') {
        _tokenizer->pop();
        _tokenizer->skip(); // skip whitespace
        _current.invalidate();
      } else {
        _current.parse();
      }
    }

    const Value& operator*() const {
      return _current;
    }

    Iterator& operator++() {
      if (_current.valid()) {
        _tokenizer->skip(); // skip whitespace
        switch (_tokenizer->peek(",]")) {
          case ',':
            _tokenizer->pop();
            _current.parse();
            break;
          case ']':
            _tokenizer->pop();
            _tokenizer->skip(); // skip whitespace
            _current.invalidate();
            break;
          default:
            _tokenizer->abort();
            _current.invalidate();
            break;
        }
      }
      return *this;
    }

    bool operator!=(const EndIterator&) const {
      return _current.valid();
    }
  };

  using Value::Value;
  
  void parse() {
    invalidate();
    if (_tokenizer) {
      _tokenizer->skip(); // skip whitespace
      if (_tokenizer->peek("[") == '[') {
        _tokenizer->pop();
        _type = ValueType::List;
      } else {
        _tokenizer->abort();
        invalidate();
      }
    }
  }

  Iterator begin() const {
    if (!valid()) {
      return {};
    }
    return {*_tokenizer};
  }

  EndIterator end() const {
    return {};
  }
};

class Property final : public Value {
public:
  using Value::Value;
  
  void parse() {
    invalidate();
    if (_tokenizer) {
      _tokenizer->skip(); // skip whitespace
      if (_tokenizer->peek("\"") == '\"') {
        _tokenizer->pop(); // remove leading "
        if (_tokenizer->nextUntil("\"", '\\') == '"') {
          _tokenizer->handleEscapedChars('\\', &jsonEscapeHandler);
          _tokenizer->storeToken(1);
          _tokenizer->pop(); // remove string contents
          _tokenizer->pop(); // remove trailing "
          _tokenizer->skip(); // skip whitespace
          if (_tokenizer->peek(":") == ':') {
            _tokenizer->pop(); // remove :
            Value::parse();
          } else {
            invalidate();
          }
        } else {
          invalidate();
        }
      } else {
        invalidate();
      }

      if (!valid()) {
        _tokenizer->abort();
      }
    }
  }
  
  toolbox::ConstStr name() const { return _tokenizer->storedToken(1); }
};

class Object final : public Value {
public:
  class Iterator;
  class EndIterator final {
  public:
    bool operator!=(const Iterator& it) const {
      return it != *this;
    }
  };

  class Iterator final {
    Tokenizer* _tokenizer;
    Property _current;

  public:
    Iterator() : _tokenizer(nullptr), _current() {}
    Iterator(Tokenizer& tokenizer) : _tokenizer(&tokenizer), _current(tokenizer) {
      _tokenizer->skip(); // skip whitespace
      if (_tokenizer->peek("}") == '}') {
        _tokenizer->pop();
        _tokenizer->skip(); // skip whitespace
        _current.invalidate();
      } else {
        _current.parse();
      }
    }

    const Property& operator*() const {
      return _current;
    }

    Iterator& operator++() {
      if (_current.valid()) {
        _tokenizer->skip(); // skip whitespace
        switch (_tokenizer->peek(",}")) {
          case ',':
            _tokenizer->pop();
            _current.parse();
            break;
          case '}':
            _tokenizer->pop();
            _tokenizer->skip(); // skip whitespace
            _current.invalidate();
            break;
          default:
            _tokenizer->abort();
            _current.invalidate();
            break;
        }
      }
      return *this;
    }
    
    bool operator!=(const EndIterator&) const {
      return _current.valid();
    }
  };

  using Value::Value;
  
  void parse() {
    invalidate();
    if (_tokenizer) {
      _tokenizer->skip(); // skip any whitespace
      if (_tokenizer->peek("{") == '{') {
        _tokenizer->pop();
        _type = ValueType::Object;
      } else {
        _tokenizer->abort();
        invalidate();
      }
    }
  }

  Iterator begin() const {
    if (!valid()) {
      return {};
    }
    return {*_tokenizer};
  }

  EndIterator end() const {
    return {};
  }
};

Object Value::asObject() const {
  if (_type != ValueType::Object) {
    return {};
  }
  Object object {*_tokenizer};
  object.parse();
  return std::move(object);
}

List Value::asList() const {
  if (_type != ValueType::List) {
    return {};
  }
  List list {*_tokenizer};
  list.parse();
  return std::move(list);
}

void Value::skip() const  {
  switch (_type) {
    case ValueType::List:
      for (auto& e : asList()) {
        e.skip();
      }
      break;
    case ValueType::Object:
      for (auto& p : asObject()) {
        p.skip();
      }
      break;
    default:
      // nothing to do
      break;
  }
}

template<typename Input, size_t max_token_length>
class Reader final {
  using Tokenizer = StoringTokenizer<Input, max_token_length, 2u>;

  Tokenizer _tokenizer;

public:
  Reader(Input& input) : _tokenizer(input) {}

  Value begin() {
    Value root {_tokenizer};
    root.parse();
    return std::move(root);
  }

  void end() {
    _tokenizer.skip();
  }

  bool failed() const {
    return _tokenizer.aborted();
  }

  // TODO provide error details for better diagnostics/logging
};

template<typename Input, size_t max_token_length = 64u>
Reader<Input, max_token_length> makeReader(Input& input) {
  return Reader<Input, max_token_length>(input);
}

}

#endif
