#ifndef JSONS_WRITER_H_
#define JSONS_WRITER_H_

#include <toolbox/Decimal.h>
#include <toolbox/Streams.h>
#include <toolbox/String.h>

namespace jsons {

class IWriter {
public:
  virtual void null() = 0;
  virtual void boolean(const toolbox::Maybe<bool>& value) = 0;
  virtual void number(const toolbox::Maybe<int32_t>& value) = 0;
  virtual void number(const toolbox::Maybe<const toolbox::Decimal&>& value) = 0;
  virtual void string(const toolbox::Maybe<const char*>& value) = 0;
  virtual void string(const toolbox::Maybe<const __FlashStringHelper*>& value) = 0;
  virtual void string(const toolbox::Maybe<toolbox::strref>& value) = 0;
  virtual void openList() = 0;
  virtual void openObject() = 0;
  virtual IWriter& property(const toolbox::strref& name) = 0;
  virtual void close() = 0;
  virtual void end() = 0;
  virtual bool failed() const = 0;
};

/**
 * Class template for writing JSON documents to a "Print-like" output with a
 * stream-oriented interface. It does not construct/store the whole document
 * before writing it which saves on allocating any buffer or dynamic memory.
 * 
 * It ensures that only valid JSON documents are constructed. If an invalid
 * operation is performed (e.g. defining a property inside a list), output
 * is aborted and the writer is marked as failed (reported by failed()). Any
 * output already written up to that point may of course already be written/sent
 * and handling with the aborted/failed JSON document must be handled in a
 * output specific way.
 * 
 * The Output type must adhere to the interface defined in IOutput, but it
 * does not actually have to have that class as a base class.
 */
template<typename Output>
class Writer final : public IWriter {
  // JSON literals
  static const char SEPARATOR = ',';
  static const char OBJECT_BEGIN = '{';
  static const char OBJECT_END = '}';
  static const char LIST_BEGIN = '[';
  static const char LIST_END = ']';
  static const char STRING_BEGIN = '"';
  static const char STRING_END = '"';
  static const char PROPERTY_BEGIN = '"';
  static const char PROPERTY_END = '"';
  static const char PROPERTY_VALUE_SEPARATOR = ':';

  // Operations
  static const uint8_t NOTHING = 0;
  static const uint8_t INSERT_VALUE = 1 << 0;
  static const uint8_t INSERT_STRING = 1 << 1;
  static const uint8_t OPEN_LIST = 1 << 2;
  static const uint8_t OPEN_OBJECT = 1 << 3;
  static const uint8_t START_PROPERTY = 1 << 4;
  static const uint8_t CLOSE = 1 << 5;

  // States of data structures
  enum struct DataStructure : uint8_t {
    None = 0,
    EmptyObject,
    Object,
    EmptyList,
    List
  };

  static const size_t MAX_STACK = 20u;

  Output& _output;
  bool _failed = false;
  DataStructure _stack[MAX_STACK] = {DataStructure::None};
  size_t _stackIndex = 0u;
  uint8_t _allowed = INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT;

  void allow(uint8_t allowed) { _allowed = allowed; }

  bool isAllowed(uint8_t op) const { return (_allowed & op) == op; }

  bool isStackAvailable() const { return _stackIndex < MAX_STACK; }

  bool isStackEmpty() const { return _stackIndex == 0u && _stack[_stackIndex] == DataStructure::None; }

  bool push(DataStructure value) {
    if (!isStackAvailable()) {
      return false;
    }
    if (!isStackEmpty()) {
      _stackIndex += 1;
    }
    _stack[_stackIndex] = value;
    return true;
  }

  DataStructure peek() { return _stack[_stackIndex]; }

  DataStructure pop() {
    if (isStackEmpty()) {
      return DataStructure::None;
    }
    _stackIndex -= 1;
    return _stack[_stackIndex + 1];
  }

  void replace(DataStructure value) {
    _stack[_stackIndex] = value;
  }

  void evaluate(uint8_t op, const toolbox::strref& value) {
    if (_failed || !isAllowed(op)) {
      _failed = true;
      return;
    }

    switch (op) {
      case INSERT_VALUE:
        if (peek() == DataStructure::List) {
          _failed = _failed || (_output.write(SEPARATOR) != 1u);
        }
        
        _failed = _failed || (_output.write(value) != value.length());

        switch (peek()) {
          case DataStructure::EmptyObject:
            replace(DataStructure::Object);
          case DataStructure::Object:
            allow(START_PROPERTY | CLOSE);
            break;
          case DataStructure::EmptyList:
            replace(DataStructure::List);
          case DataStructure::List:
            allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
            break;
          case DataStructure::None:
            allow(NOTHING);
            break;
          default:
            _failed = true;
            break;
        }
        break;
      case INSERT_STRING:
        if (peek() == DataStructure::List) {
          _failed = _failed || (_output.write(SEPARATOR) != 1u);
        }

        _failed = _failed || (_output.write(STRING_BEGIN) != 1u);
        {
          const size_t length = value.length();
          size_t i = 0;
          while (!_failed && i < length) {
            char c = value.charAt(i);
            switch (c)
            {
            case '\\':
            case '"':
              _failed = _failed || (_output.write('\\') != 1);
            default:
              _failed = _failed || (_output.write(c) != 1);
              break;
            }
            ++i;
          }
        }
        _failed = _failed || (_output.write(STRING_END) != 1u);

        switch (peek()) {
          case DataStructure::EmptyObject:
            replace(DataStructure::Object);
          case DataStructure::Object:
            allow(START_PROPERTY | CLOSE);
            break;
          case DataStructure::EmptyList:
            replace(DataStructure::List);
          case DataStructure::List:
            allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
            break;
          case DataStructure::None:
            allow(NOTHING);
            break;
          default:
            _failed = true;
            break;
        }
        break;
      case OPEN_LIST:
        switch (peek()) {
          case DataStructure::EmptyList:
            replace(DataStructure::List);
            break;
          case DataStructure::List:
            _failed = _failed || (_output.write(SEPARATOR) != 1u);
            break;
          default:
            // nothing
            break;
        }

        _failed = _failed || (_output.write(LIST_BEGIN) != 1u);
        _failed = _failed || !push(DataStructure::EmptyList);
        allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
        break;
      case OPEN_OBJECT:
        switch (peek()) {
          case DataStructure::EmptyList:
            replace(DataStructure::List);
            break;
          case DataStructure::List:
            _failed = _failed || (_output.write(SEPARATOR) != 1u);
            break;
          default:
            // nothing
            break;
        }
        
        _failed = _failed || (_output.write(OBJECT_BEGIN) != 1u);
        _failed = _failed || !push(DataStructure::EmptyObject);
        allow(START_PROPERTY | CLOSE);
        break;
      case START_PROPERTY:
        switch (peek()) {
          case DataStructure::EmptyObject:
            replace(DataStructure::Object);
            break;
          case DataStructure::Object:
            _failed = _failed || (_output.write(SEPARATOR) != 1u);
            break;
          default:
            _failed = true;
            break;
        }
        _failed = _failed || (_output.write(PROPERTY_BEGIN) != 1u);
        {
          const size_t length = value.length();
          size_t i = 0;
          while (!_failed && i < length) {
            char c = value.charAt(i);
            switch (c)
            {
            case '\\':
            case '"':
              _failed = _failed || (_output.write('\\') != 1);
            default:
              _failed = _failed || (_output.write(c) != 1);
              break;
            }
            ++i;
          }
        }
        _failed = _failed || (_output.write(PROPERTY_END) != 1u);
        _failed = _failed || (_output.write(PROPERTY_VALUE_SEPARATOR) != 1u);
        allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT);
        break;
      case CLOSE:
        switch (pop()) {
          case DataStructure::EmptyObject:
          case DataStructure::Object:
            _failed = _failed || (_output.write(OBJECT_END) != 1u);
            switch (peek()) {
              case DataStructure::EmptyObject:
              case DataStructure::Object:
                allow(START_PROPERTY | CLOSE);
                break;
              case DataStructure::EmptyList:
              case DataStructure::List:
                allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
                break;
              case DataStructure::None:
                allow(NOTHING);
                break;
            }
            break;
          case DataStructure::EmptyList:
          case DataStructure::List:
            _failed = _failed || (_output.write(LIST_END) != 1u);
            switch (peek()) {
              case DataStructure::EmptyObject:
              case DataStructure::Object:
                allow(START_PROPERTY | CLOSE);
                break;
              case DataStructure::EmptyList:
              case DataStructure::List:
                allow(INSERT_VALUE | INSERT_STRING | OPEN_LIST | OPEN_OBJECT | CLOSE);
                break;
              case DataStructure::None:
                allow(NOTHING);
                break;
            }
            break;
          default:
            _failed = true; // should not happen
            break;
        }
        break;
      default:
        _failed = true;
        break;
    }
  }

public:
  Writer(Output& output) : _output(output) {}

  ~Writer() {
    end();
  }

  void null() override {
    evaluate(INSERT_VALUE, "null");
  }

  void boolean(const toolbox::Maybe<bool>& value) override {
    if (value) {
      evaluate(INSERT_VALUE, value.get() ? "true" : "false");
    } else {
      null();
    }
  }

  void number(const toolbox::Maybe<int32_t>& value) override {
    static char buffer[12];
    if (value) {
      snprintf(buffer, std::size(buffer), "%i", value.get());
      evaluate(INSERT_VALUE, buffer);
    } else {
      null();
    }
  }
  
  void number(const toolbox::Maybe<const toolbox::Decimal&>& value) override {
    if (value) {
      evaluate(INSERT_VALUE, value.get().toString());
    } else {
      null();
    }
  }
  
  void string(const toolbox::Maybe<const char*>& value) override {
    if (value) {
      evaluate(INSERT_STRING, value.get());
    } else {
      null();
    }
  }
  
  void string(const toolbox::Maybe<const __FlashStringHelper*>& value) override {
    if (value) {
      evaluate(INSERT_STRING, value.get());
    } else {
      null();
    }
  }

  void string(const toolbox::Maybe<toolbox::strref>& value) override {
    if (value) {
      evaluate(INSERT_STRING, value.get());
    } else {
      null();
    }
  }

  void openList() override {
    evaluate(OPEN_LIST, "");
  }

  void openObject() override {
    evaluate(OPEN_OBJECT, "");
  }

  IWriter& property(const toolbox::strref& name) override {
    evaluate(START_PROPERTY, name);
    return *this;
  }

  void close() override {
    evaluate(CLOSE, "");
  }

  void end() override {
    while (!failed() && peek() != DataStructure::None) {
      close();
    }
  }

  bool failed() const override { return _failed; }
};

template<typename Output>
Writer<Output> makeWriter(Output& output) { return {output}; }

}

#endif
