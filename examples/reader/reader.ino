#include <Arduino.h>

#include <jsons.h>
#include <toolbox/Streams.h>

void printJson(jsons::Value& value) {
  switch (value.type())
  {
  case jsons::ValueType::Null:
    Serial.println("null");
    break;
  case jsons::ValueType::Boolean:
    Serial.println(value.asBoolean().get() ? "true" : "false");
    break;
  case jsons::ValueType::Integer:
    Serial.println(value.asInteger().get());
    break;
  case jsons::ValueType::Decimal:
    Serial.println(value.asDecimal().get().toString().toString());
    break;
  case jsons::ValueType::String:
    Serial.println(value.asString().get().toString());
    break;
  case jsons::ValueType::List:
    Serial.println("[");
    for (auto& e : value.asList()) {
      printJson(e);
    }
    Serial.println("]");
    break;
  case jsons::ValueType::Object:
    Serial.println("{");
    for (auto& p : value.asObject()) {
      Serial.print(p.name().toString());
      Serial.print(": ");
      printJson(p);
    }
    Serial.println("}");
    break;
  default:
    Serial.println("INVALID");
    break;
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  Serial.println();
  Serial.println("JSON:");

  toolbox::StringInput input{F("{\"prop1\" : \"test \\\"string\\\"\", \n\"prop2\": 123.01 , \"prop3\" : []\n, \"prop4\": null,\"prop5\": true, \"prop6\": false, \"prop7\": { }, \"prop7\": [ 1, 2,3, \"test\", {\"prop1\": 4.1} ] }")};
  auto reader = jsons::makeReader(input);
  auto json = reader.begin();
  printJson(json);
  reader.end();

  Serial.println();

  if (reader.failed()) {
    Serial.println("JSON reader failed.");
    auto diagnostics = reader.diagnostics();
    Serial.print("Error at position ");
    Serial.print(diagnostics.streamPosition);
    Serial.print(" in input: ");
    if (diagnostics.errorMessage.isInProgmem()) {
      Serial.println(diagnostics.errorMessage.fpstr());
    } else {
      Serial.println(diagnostics.errorMessage.cstr());
    }
    Serial.println("Next characters: ");
    Serial.println(diagnostics.bufferContents.cstr());
  } else {
    Serial.println("JSON reader successful.");
  }
}

void loop() {
}
