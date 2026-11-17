# Third-party notices

## Waveshare e-Paper example

The driver, drawing helpers, and font sources in
`firmware/SEO_Dashboard/` originate from Waveshare's Arduino
`epd2in9_V2` example:

- Upstream repository: https://github.com/waveshareteam/e-Paper
- Reference commit: `a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc`
- Example: https://github.com/waveshareteam/e-Paper/tree/a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc/Arduino/epd2in9_V2

The imported files retain their original copyright and license notices. The
local versions include ESP32-C3 pin/SPI adaptation, ESP32-compatible font
includes, and bounded display diagnostics and recovery. They are not presented
as unmodified upstream files. The original LUT and full-update sequence are
retained.

The font files carry the separate STMicroelectronics notice reproduced below;
the Waveshare permission notice does not replace it.

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

## STMicroelectronics bitmap fonts

`fonts.h`, `font8.cpp`, `font12.cpp`, `font20.cpp`, and `font24.cpp` were included
through the Waveshare example. Their headers credit the STMicroelectronics MCD
Application Team, dated 18 February 2014. The original three-condition notice
remains in the source files and is reproduced here for binary distributions:

```text
COPYRIGHT(c) 2014 STMicroelectronics

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of STMicroelectronics nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## ESP32 Arduino core

The firmware uses libraries bundled with Espressif's ESP32 Arduino core,
including Wi-Fi, HTTP/TLS, SPI, Preferences, and cJSON. These dependencies are
installed through the ESP32 board package and are not vendored here. Their
upstream licenses continue to apply:
https://github.com/espressif/arduino-esp32

## Hardware CAD

The published STL, STEP, and FreeCAD assemblies contain the original enclosure
design only. Third-party ESP32 and display STEP models are not distributed,
including inside the native assembly. The former board-reference viewer and
its Three.js bundle are also absent from the public project.

Enclosure dimensions reflect the boards used for the prototype; this does not
make the electronic board designs part of the project's MIT-licensed work.
