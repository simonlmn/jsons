
#include <cstdio>
#include <cassert>

#include "../src/jsons.h"

void printJson(const jsons::Value& value) {
  switch (value.type())
  {
  case jsons::ValueType::Null:
    printf("null\n");
    break;
  case jsons::ValueType::Boolean:
    printf("%s\n", value.asBoolean().value() ? "true" : "false");
    break;
  case jsons::ValueType::Integer:
    printf("%i\n", value.asInteger().value());
    break;
  case jsons::ValueType::Decimal:
    printf("%s\n", value.asDecimal().value().toString());
    break;
  case jsons::ValueType::String:
    printf("%s\n", value.asString().value());
    break;
  case jsons::ValueType::List:
    printf("[\n");
    for (auto& e : value.asList()) {
      printJson(e);
    }
    printf("]\n");
    break;
  case jsons::ValueType::Object:
    printf("{\n");
    for (auto& p : value.asObject()) {
      printf("%s: ", p.name());
      printJson(p);
    }
    printf("}\n");
    break;
  default:
    printf("INVALID");
    break;
  }
}

int main() {
  toolbox::StringInput input{"{\"prop1\" : \"test \\\"string\\\"\", \n\"prop2\": 123.01 , \"prop3\" : []\n, \"prop4\": null,\"prop5\": true, \"prop6\": false, \"prop7\": { }, \"prop7\": [ 1, 2,3, \"test\", {\"prop1\": 4.1} ] }"};
  auto reader = jsons::makeReader(input);
  printJson(reader.begin());
  reader.end();

  char buffer[512] {'\0'};
  toolbox::StringOutput output{buffer};
  auto writer = jsons::makeWriter(output);
  writer.openObject();
  writer.property("prop1").string("test");
  writer.property("prop2").boolean(true);
  writer.property("prop3").number(123);
  writer.property("prop4").number(toolbox::Decimal{234, 1, 2});
  writer.property("prop5").openObject();
    writer.property("a\"c").openList();
      writer.string("some \"text\"");
      writer.string("some more text");
      writer.null();
      writer.close();
    writer.close();
  writer.close();
  printf("%s\n", buffer);

  return 0;
}