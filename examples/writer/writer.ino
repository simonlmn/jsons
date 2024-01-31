#include <Arduino.h>

#include <jsons.h>
#include <toolbox/Streams.h>

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  toolbox::PrintOutput output{Serial};
  output.write("\n");
  output.write("JSON:\n");

  auto writer = jsons::makeWriter(output);
  writer.openObject();
  writer.property("string").string("test");
  writer.property("boolean").boolean(true);
  writer.property("integer").number(123);
  writer.property("decimal").number(toolbox::Decimal{234, 1, 2});
  writer.property("object").openObject();
    writer.property("list").openList();
      writer.string("some text");
      writer.string("some more text");
      writer.null();
      writer.close();
    writer.close();
  writer.close();
  writer.end();
  
  output.write("\n");

  if (writer.failed()) {
    output.write("JSON writer failed.\n");
  } else {
    output.write("JSON writer successful.\n");
  }
}

void loop() {
}
