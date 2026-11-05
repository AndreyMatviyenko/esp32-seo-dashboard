# Third-party notices

## Waveshare e-Paper example

The driver, drawing helpers, and font sources in
`firmware/Webfonts_AdSense_Live/` originate from Waveshare's Arduino
`epd2in9_V2` example:

- Upstream repository: https://github.com/waveshareteam/e-Paper
- Reference commit: `a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc`
- Example: https://github.com/waveshareteam/e-Paper/tree/a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc/Arduino/epd2in9_V2

The imported files retain their original copyright and license notices. The
local versions include ESP32-C3 pin/SPI adaptation, ESP32-compatible font
includes, and bounded display diagnostics and recovery. They are not presented
as unmodified upstream files. The original LUT and full-update sequence are
retained.

The upstream permission notice is reproduced below. Individual source notices
also remain in place.

```text
Copyright (C) Waveshare     August 10 2017

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documnetation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## ESP32 Arduino core

The firmware uses libraries bundled with Espressif's ESP32 Arduino core,
including Wi-Fi, HTTP/TLS, SPI, Preferences, and cJSON. These dependencies are
installed through the ESP32 board package and are not vendored here. Their
upstream licenses continue to apply:
https://github.com/espressif/arduino-esp32
