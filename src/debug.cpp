#include "common.h"

void Debug::printLn(String debugText)
{
  // Debug output line of text to our debug targets
  String debugTimeText = "[+" + String(float(millis()) / 1000, 3) + "s] " + debugText;
  Serial.println(debugTimeText);
  web.telnetPrintLn(_telnetEnabled, debugTimeText);
}

void Debug::print(String debugText)
{ // Debug output single character to our debug targets (DON'T USE THIS!)
  // Try to avoid using this function if at all possible.  When connected to telnet, printing each
  // character requires a full TCP round-trip + acknowledgement back and execution halts while this
  // happens.  Far better to put everything into a line and send it all out in one packet using
  // debugPrintln.
  Serial.print(debugText);
  web.telnetPrint(_telnetEnabled, debugText);
}