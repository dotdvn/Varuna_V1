#ifndef VARUNA_DOTFRAME_MODE_H
#define VARUNA_DOTFRAME_MODE_H

#include <Adafruit_ST7735.h>

namespace DotFrameMode {
bool begin(Adafruit_ST7735 &display);
void loop();
bool isActive();
} // namespace DotFrameMode

#endif
