#ifndef JSONS_TOKENIZER_H_
#define JSONS_TOKENIZER_H_

#include <cstring>
#include <algorithm>
#include <toolbox/Streams.h>
#include <toolbox/String.h>

namespace jsons {

static size_t defaultEscapeHandler(const char** source, char** destination) {
  ++(*source);
  **destination = **source;
  return 1u;
}

class ITokenizer {
public:
  virtual size_t maxTokenLength() const = 0;
  virtual void abort(const toolbox::strref& reason) = 0;
  virtual bool aborted() const = 0;
  virtual const toolbox::strref& abortReason() const = 0;
  virtual bool completed() const = 0;
  virtual size_t positionInInput() const = 0;
  virtual char stopChar() const = 0;
  virtual const char* current() const = 0;
  virtual char peek(const char* chars) = 0;
  virtual void pop() = 0;
  virtual void skip(const char* chars = " \r\n\t") = 0;
  virtual char nextUntil(const char* stopChars, char escapeChar = '\0') = 0;
  virtual char nextWhile(const char* stopChars, char escapeChar = '\0') = 0;
  virtual void handleEscapedChars(char escapeChar, std::function<size_t(const char** source, char** destination)> handler = &defaultEscapeHandler) = 0;
};

template<typename Input, size_t max_token_length, typename Interface = ITokenizer>
class Tokenizer : public Interface {
public:
  static const size_t MAX_TOKEN_LENGTH = max_token_length;

private:
  Input& _input;
  size_t _inputCharsRead;
  bool _aborted;
  toolbox::strref _abortReason;
  char _buffer[MAX_TOKEN_LENGTH + 1]; // add terminating zero
  size_t _bufferLength;
  size_t _stopPosition;
  char _stopChar;

  void shiftBufferToStopPosition() {
    if (_bufferLength > 0 && _buffer[_stopPosition] == '\0') {
      size_t bufferSizeToShift = _bufferLength - _stopPosition + 1; // includes terminating zero
      memmove(_buffer, _buffer + _stopPosition, bufferSizeToShift);
      _bufferLength = bufferSizeToShift - 1; // subtract terminating zero
      _stopPosition = 0;
      _buffer[_stopPosition] = _stopChar;
      _stopChar = '\0';
    }
  }

  void fillBuffer() {
    if (_bufferLength < MAX_TOKEN_LENGTH && _input.available() > 0) {
      size_t charsRead = _input.readString(_buffer + _bufferLength, MAX_TOKEN_LENGTH - _bufferLength);
      _inputCharsRead += charsRead;
      _bufferLength += charsRead;
      _buffer[_bufferLength] = '\0';
    }
  }

  /**
   * Function to check if the character at the current _stopPosition
   * is preceeded by an odd number of escape characters.
   */
  bool isEscaped(char escapeChar) const {
    if (_stopPosition == 0 || _stopChar == '\0') {
      return false;
    }

    bool escaped = false;

    size_t i = _stopPosition;
    while (i > 0) {
      if (_buffer[i - 1] == escapeChar) {
        escaped = !escaped;
      } else {
        break;
      }
      --i;
    }

    return escaped;
  }

public:
  Tokenizer(Input& input) : _input(input), _inputCharsRead(0), _aborted(false), _abortReason(), _buffer(), _bufferLength(0), _stopPosition(0), _stopChar('\0') {
  }

  size_t maxTokenLength() const override {
    return MAX_TOKEN_LENGTH;
  }

  void abort(const toolbox::strref& reason) override {
    _aborted = true;
    _abortReason = reason;
  }

  bool aborted() const override {
    return _aborted;
  }

  const toolbox::strref& abortReason() const {
    return _abortReason;
  }

  bool completed() const override {
    return !aborted() && _input.available() == 0;
  }

  size_t positionInInput() const override {
    return _inputCharsRead - _bufferLength;
  }

  char stopChar() const override {
    return _stopChar;
  }

  const char* current() const override {
    return _buffer;
  }

  char peek(const char* chars) override {
    if (aborted()) {
      return '\0';
    }

    shiftBufferToStopPosition();
    fillBuffer();

    if (strchr(chars, _buffer[0])) {
      return _buffer[0];
    } else {
      return '\0';
    }
  }

  void pop() override {
    if (aborted()) {
      return;
    }

    if (_stopPosition == 0 && _buffer[_stopPosition] != '\0') {
      _stopPosition = 1;
      _stopChar = _buffer[_stopPosition];
      _buffer[_stopPosition] = '\0';
    }

    shiftBufferToStopPosition();
    fillBuffer();
  }

  char nextUntil(const char* stopChars, char escapeChar = '\0') override {
    if (aborted()) {
      return '\0';
    }

    shiftBufferToStopPosition();
    fillBuffer();

    while (_buffer[_stopPosition] != '\0') {
      _stopPosition = _stopPosition + strcspn(_buffer + _stopPosition, stopChars);
      _stopChar = _buffer[_stopPosition];
      if (isEscaped(escapeChar)) {
        _stopPosition += 1;
      } else {
        break;
      }
    };

    _buffer[_stopPosition] = '\0';
    return _stopChar;
  }

  char nextWhile(const char* stopChars, char escapeChar = '\0') override {
    if (aborted()) {
      return '\0';
    }

    shiftBufferToStopPosition();
    fillBuffer();

    while (_buffer[_stopPosition] != '\0') {
      _stopPosition = _stopPosition + strspn(_buffer + _stopPosition, stopChars);
      _stopChar = _buffer[_stopPosition];
      if (isEscaped(escapeChar)) {
        _stopPosition += 1;
      } else {
        break;
      }
    };
    
    _buffer[_stopPosition] = '\0';
    return _stopChar;
  }

  void skip(const char* chars = " \r\n\t") override {
    if (aborted()) {
      return;
    }

    do {
      shiftBufferToStopPosition();
      fillBuffer();
      _stopPosition = strspn(_buffer, chars);
    } while (_buffer[_stopPosition] == '\0' && _input.available() > 0);

    if (_stopPosition > 0 && strchr(chars, _buffer[_stopPosition - 1])) {
      _stopChar = _buffer[_stopPosition];
      _buffer[_stopPosition] = '\0';
      shiftBufferToStopPosition();
    }
  }

  void handleEscapedChars(char escapeChar, std::function<size_t(const char**, char**)> handler = &defaultEscapeHandler) override {
    if (_bufferLength > 0 && _buffer[_stopPosition] == '\0') {
      const char* source = &_buffer[0];
      const char* sourceEnd = &_buffer[_stopPosition];
      char* destination = &_buffer[0];
      size_t skippedChars = 0;
      while (source < sourceEnd) {
        if (*source == escapeChar) {
          skippedChars += handler(&source, &destination);
        } else {
          *destination = *source;
        }
        ++destination;
        ++source;
      }
      *destination = *sourceEnd;

      memmove(destination + 1, sourceEnd + 1, _bufferLength - _stopPosition - 1);

      _stopPosition -= skippedChars;
      _bufferLength -= skippedChars;
    }
  }
};

class IStoringTokenizer : public ITokenizer {
public:
  virtual size_t maxTokens() const = 0;
  virtual void storeToken(size_t index) = 0;
  virtual toolbox::strref storedToken(size_t index) const = 0;
};

template<typename Input, size_t max_token_length, size_t max_tokens>
class StoringTokenizer final : public Tokenizer<Input, max_token_length, IStoringTokenizer> {
public:
  static const size_t MAX_TOKENS = max_tokens;

private:
  char _tokenStorage[MAX_TOKENS][Tokenizer<Input, max_token_length, IStoringTokenizer>::MAX_TOKEN_LENGTH + 1];

public:
  using Tokenizer<Input, max_token_length, IStoringTokenizer>::Tokenizer;

  size_t maxTokens() const override {
    return MAX_TOKENS;
  }

  void storeToken(size_t index) override {
    if (index < MAX_TOKENS) {
      strcpy(_tokenStorage[index], this->current());
    }
  }

  toolbox::strref storedToken(size_t index) const override {
    if (index < MAX_TOKENS) {
      return _tokenStorage[index];
    } else {
      return {};
    }
  }
};

}

#endif