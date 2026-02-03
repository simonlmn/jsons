#include <yatest.h>
#include <jsons/Writer.h>
#include <toolbox/String.h>
#include <sstream>

namespace {
  struct Out {
    std::stringstream output;

    size_t write(char c) { output << c; return 1; }
    size_t write(const char* str) { 
      if (!str) return 0;
      output << str; 
      return strlen(str); 
    }
    size_t write(const toolbox::strref& str) {
      for (size_t i = 0; i < str.length(); i++) {
        write(str.charAt(i));
      }
      return str.length();
    }
  };

  using JsonWriter = jsons::Writer<Out>;

  template<typename TestCaseFunction>
  void validJson(TestCaseFunction testCase, const std::string& expected) {
    Out o;
    {
      JsonWriter writer(o);
      testCase(writer);
      if (writer.failed()) {
        throw std::runtime_error("writer failed");
      }
    }
    std::string result = o.output.str();
    if (result != expected) {
      throw std::runtime_error(std::string("output mismatch: got '") + result + "', expected '" + expected + "'");
    }
  }

  template<typename TestCaseFunction>
  void invalidJson(TestCaseFunction testCase) {
    Out o;
    {
      JsonWriter writer(o);
      testCase(writer);
      if (!writer.failed()) {
        throw std::runtime_error("writer was expected to fail but succeeded");
      }
    }
  }

  static const yatest::TestSuite& TestJsonWriter =
  yatest::suite("JsonWriter")
    .tests("empty document", []() {
      validJson([] (JsonWriter& writer) {
        writer.end();
      }, "");
    })
    .tests("plain value", []() {
      validJson([] (JsonWriter& writer) {
        writer.number(123);
      }, "123");
    })
    .tests("string value", []() {
      validJson([] (JsonWriter& writer) {
        writer.string("123");
      }, "\"123\"");
    })
    .tests("empty object", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
      }, "{}");
    })
    .tests("empty list", []() {
      validJson([] (JsonWriter& writer) {
        writer.openList();
      }, "[]");
    })
    .tests("list with plain value", []() {
      validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.number(123);
      }, "[123]");
    })
    .tests("list with plain and string value", []() {
      validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.number(123);
        writer.string("123");
      }, "[123,\"123\"]");
    })
    .tests("list with plain, string and nested list value", []() {
      validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.number(123);
        writer.string("123");
        writer.openList();
      }, "[123,\"123\",[]]");
    })
    .tests("list with empty object value", []() {
      validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.openObject();
      }, "[{}]");
    })
    .tests("list with multiple nested empty list values", []() {
      validJson([] (JsonWriter& writer) {
        writer.openList();
        writer.openList();
        writer.close();
        writer.openList();
        writer.close();
        writer.openList();
      }, "[[],[],[]]");
    })
    .tests("object with number value property", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop").number(123);
      }, "{\"prop\":123}");
    })
    .tests("object with string value property", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop").string("123");
      }, "{\"prop\":\"123\"}");
    })
    .tests("object with number value property (separate construction)", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.number(123);
      }, "{\"prop\":123}");
    })
    .tests("object with empty list property", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openList();
      }, "{\"prop\":[]}");
    })
    .tests("object with non-empty list property", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openList();
        writer.number(123);
        writer.number(234);
      }, "{\"prop\":[123,234]}");
    })
    .tests("object with empty object property", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
      }, "{\"prop\":{}}");
    })
    .tests("object with another property after an empty object property", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
        writer.close();
        writer.property("prop2").number(123);
      }, "{\"prop\":{},\"prop2\":123}");
    })
    .tests("object with non-empty object property", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.openObject();
        writer.property("prop2").number(123);
      }, "{\"prop\":{\"prop2\":123}}");
    })
    .tests("object with multiple number value properties", []() {
      validJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop1").number(123);
        writer.property("prop2").number(234);
        writer.property("prop3").number(toolbox::Decimal::fromFixedPoint(12, 1));
      }, "{\"prop1\":123,\"prop2\":234,\"prop3\":1.2}");
    })
    .tests("top-level multiple number values are not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.number(123);
        writer.number(123);
      });
    })
    .tests("top-level string and number values are not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.string("123");
        writer.number(123);
      });
    })
    .tests("string value within an object (without property) is not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.string("123");
      });
    })
    .tests("list within an object (without property) is not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.openList();
      });
    })
    .tests("top-level object and number value is not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.close();
        writer.number(123);
      });
    })
    .tests("object with unfinished property is not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.openObject();
        writer.property("prop");
        writer.close();
      });
    })
    .tests("top-level property is not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.property("prop");
      });
    })
    .tests("list with property is not allowed", []() {
      invalidJson([] (JsonWriter& writer) {
        writer.openList();
        writer.property("prop");
      });
    });
}
