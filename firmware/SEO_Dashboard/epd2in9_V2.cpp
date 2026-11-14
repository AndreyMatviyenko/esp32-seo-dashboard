/**
 *  @filename   :   epd2in9_V2.cpp
 *  @brief      :   Implements for e-paper library
 *  @author     :  
 *
 *  Copyright (C) Waveshare     Nov 9 2020
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "epd2in9_V2.h"

unsigned char _WF_PARTIAL_2IN9[159] =
{
0x0,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x40,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0A,0x0,0x0,0x0,0x0,0x0,0x2,  
0x1,0x0,0x0,0x0,0x0,0x0,0x0,
0x1,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x0,0x0,0x0,0x0,0x0,0x0,0x0,
0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
0x22,0x17,0x41,0xB0,0x32,0x36,
};

unsigned char WS_20_30[159] =
{											
0x80,	0x66,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x40,	0x0,	0x0,	0x0,
0x10,	0x66,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x20,	0x0,	0x0,	0x0,
0x80,	0x66,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x40,	0x0,	0x0,	0x0,
0x10,	0x66,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x20,	0x0,	0x0,	0x0,
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,
0x14,	0x8,	0x0,	0x0,	0x0,	0x0,	0x1,					
0xA,	0xA,	0x0,	0xA,	0xA,	0x0,	0x1,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x14,	0x8,	0x0,	0x1,	0x0,	0x0,	0x1,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x1,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x0,	0x0,	0x0,	0x0,	0x0,	0x0,	0x0,					
0x44,	0x44,	0x44,	0x44,	0x44,	0x44,	0x0,	0x0,	0x0,			
0x22,	0x17,	0x41,	0x0,	0x32,	0x36
};	

unsigned char Gray4[159] =			
{											
0x00,	0x60,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L0	 //2.28s			
0x20,	0x60,	0x10,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L1				
0x28,	0x60,	0x14,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L2				
0x2A,	0x60,	0x15,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L3 				
0x00,	0x90,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L4 				
0x00,	0x02,	0x00,	0x05,	0x14,	0x00,	0x00,						//TP, SR, RP of Group0				
0x1E,	0x1E,	0x00,	0x00,	0x00,	0x00,	0x01,						//TP, SR, RP of Group1				
0x00,	0x02,	0x00,	0x05,	0x14,	0x00,	0x00,						//TP, SR, RP of Group2				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group3				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group4				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group5				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group6				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group7				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group8				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group9				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group10				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group11				
0x24,	0x22,	0x22,	0x22,	0x23,	0x32,	0x00,	0x00,	0x00,				//FR, XON				
0x22,	0x17,	0x41,	0xAE,	0x32,	0x28,							//EOPT VGH VSH1 VSH2 VSL VCOM				
};

unsigned char WF_FULL[159] =
{											
0x90,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L0	1.00S			
0x60,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L1				
0x90,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L2				
0x60,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L3 				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	//VS L4 				
0x19,	0x19,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group0				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group1				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group2				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group3				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group4				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group5				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group6				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group7				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group8				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group9				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group10				
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,						//TP, SR, RP of Group11				
0x24,	0x42,	0x22,	0x22,	0x23,	0x32,	0x00,	0x00,	0x00,				//FR, XON				
0x22,	0x17,	0x41,	0xAE,	0x32,	0x38,							//EOPT VGH VSH1 VSH2 VSL VCOM				
};

Epd::~Epd() {
};

Epd::Epd() {
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;
    last_error = EpdError::None;
    init_diagnostics = {};
};

int Epd::Init() {
    last_error = EpdError::None;
    init_diagnostics = {};
    /* this calls the peripheral hardware interface, see epdif */
    if (IfInit() != 0) {
        last_error = EpdError::SpiInitFailed;
        return -1;
    }
	
	Reset();
	
    /* EPD hardware init start */
	if (!WaitUntilIdle(10000, &init_diagnostics.reset)) return -1;
	SendCommand(0x12);  //SWRESET
	if (!WaitUntilIdle(10000, &init_diagnostics.softwareReset)) return -1;
	
	SendCommand(0x01); //Driver output control      
	SendData(0x27);
	SendData(0x01);
	SendData(0x00);
	
	SendCommand(0x11); //data entry mode       
	SendData(0x03);

	SetMemoryArea(0, 0, width-1, height-1);

	SendCommand(0x21); //  Display update control
	SendData(0x00);
	SendData(0x80);	

	SetMemoryPointer(0, 0);
	if (!WaitUntilIdle()) return -1;

    SetLut_by_host(WS_20_30);
    /* EPD hardware init end */
    return HasError() ? -1 : 0;
}

int Epd::Init_Fast() {
    last_error = EpdError::None;
    init_diagnostics = {};
    /* this calls the peripheral hardware interface, see epdif */
    if (IfInit() != 0) {
        last_error = EpdError::SpiInitFailed;
        return -1;
    }
	
	Reset();
	
    /* EPD hardware init start */
	if (!WaitUntilIdle()) return -1;   
	SendCommand(0x12);  //SWRESET
	if (!WaitUntilIdle()) return -1;   
	
	SendCommand(0x01); //Driver output control      
	SendData(0x27);
	SendData(0x01);
	SendData(0x00);
	
	SendCommand(0x11); //data entry mode       
	SendData(0x03);

	SetMemoryArea(0, 0, width-1, height-1);

    SendCommand(0x3C);       
	SendData(0x05);

	SendCommand(0x21); //  Display update control
	SendData(0x00);
	SendData(0x80);	

	SetMemoryPointer(0, 0);
	if (!WaitUntilIdle()) return -1;

    SetLut_by_host(WF_FULL);
    /* EPD hardware init end */
    return HasError() ? -1 : 0;
}

int Epd::Init_4Gray() {
    last_error = EpdError::None;
    init_diagnostics = {};
    /* this calls the peripheral hardware interface, see epdif */
    if (IfInit() != 0) {
        last_error = EpdError::SpiInitFailed;
        return -1;
    }
	
	Reset();
	
    /* EPD hardware init start */
	if (!WaitUntilIdle()) return -1;   
	SendCommand(0x12);  //SWRESET
	if (!WaitUntilIdle()) return -1;   
	
	SendCommand(0x01); //Driver output control      
	SendData(0x27);
	SendData(0x01);
	SendData(0x00);
	
	SendCommand(0x11); //data entry mode       
	SendData(0x03);

	SetMemoryArea(8, 0, width, height-1);

	SendCommand(0x3C); 
	SendData(0x04);

	SetMemoryPointer(8, 0);
	if (!WaitUntilIdle()) return -1;

  SetLut_by_host(Gray4);
  /* EPD hardware init end */
  return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command) {
    if (HasError()) return;
    DigitalWrite(dc_pin, LOW);
    DigitalWrite(cs_pin, LOW);
    SpiTransfer(command);
    DigitalWrite(cs_pin, HIGH);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data) {
    if (HasError()) return;
    DigitalWrite(dc_pin, HIGH);
    DigitalWrite(cs_pin, LOW);
    SpiTransfer(data);
    DigitalWrite(cs_pin, HIGH);
}

/**
 *  @brief: Wait until the busy_pin goes LOW
 */
bool Epd::HasError(void) const {
    return last_error != EpdError::None;
}

EpdError Epd::GetError(void) const {
    return last_error;
}

const EpdInitDiagnostics& Epd::GetInitDiagnostics(void) const {
    return init_diagnostics;
}

int Epd::ObserveBusy(EpdBusyTrace* trace) {
    const int level = DigitalRead(busy_pin);
    if (trace) {
        if (!trace->sampled) trace->first = level;
        trace->sampled = true;
        trace->last = level;
        trace->highSeen = trace->highSeen || level == HIGH;
    }
    return level;
}

const char* Epd::GetLastError(void) const {
    switch (last_error) {
        case EpdError::None: return "NONE";
        case EpdError::SpiInitFailed: return "SPI_INIT_FAILED";
        case EpdError::BusyTimeout: return "BUSY_TIMEOUT";
        case EpdError::RefreshNotStarted: return "REFRESH_NOT_STARTED";
    }
    return "UNKNOWN";
}

bool Epd::WaitUntilIdle(unsigned long timeout_ms, EpdBusyTrace* trace) {
    if (HasError()) return false;
    ObserveBusy(trace);
    // Give BUSY time to assert after commands such as software reset.
    DelayMs(1);
    const uint32_t started = millis();
    uint32_t idleStarted = started;
    bool idle = false;
    while (true) {
        const uint32_t now = millis();
        if (ObserveBusy(trace) == LOW) {
            if (!idle) idleStarted = now;
            idle = true;
            if ((uint32_t)(now - idleStarted) >= 5) return true;
        } else {
            idle = false;
        }
        if ((uint32_t)(millis() - started) >= timeout_ms) {
            last_error = EpdError::BusyTimeout;
            return false;
        }
        DelayMs(1);
    }
}

bool Epd::CanRetryRefresh(unsigned long guard_ms) {
    if (last_error != EpdError::RefreshNotStarted) return false;
    // An assertion just after the 250ms deadline must not trigger another reset.
    // This observation is bounded; it is not controller readback.
    const uint32_t started = millis();
    do {
        if (DigitalRead(busy_pin) == HIGH) return false;
        if ((uint32_t)(millis() - started) >= guard_ms) return true;
        DelayMs(1);
    } while (true);
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see Epd::Sleep();
 */
void Epd::Reset(void) {
    DigitalWrite(reset_pin, HIGH);
    DelayMs(20);  
    DigitalWrite(reset_pin, LOW);                //module reset    
    const uint32_t lowStarted = micros();
    // Short pulse for the Waveshare power-switch/reset circuit.
    // Busy-wait avoids intentionally yielding the LOW pulse to the RTOS tick.
    delayMicroseconds(2000);
    DigitalWrite(reset_pin, HIGH);
    init_diagnostics.resetLowUs = (uint32_t)(micros() - lowStarted);
    // Sample during the existing post-reset settling interval, without UART I/O.
    const uint32_t released = millis();
    do {
        ObserveBusy(&init_diagnostics.reset);
        if ((uint32_t)(millis() - released) >= 20) break;
        DelayMs(1);
    } while (true);
}

/**
 *  @brief: put an image buffer to the frame memory.
 *          this won't update the display.
 */
void Epd::SetFrameMemory(
    const unsigned char* image_buffer,
    int x,
    int y,
    int image_width,
    int image_height
) {
    int x_end;
    int y_end;

    if (
        image_buffer == NULL ||
        x < 0 || image_width < 0 ||
        y < 0 || image_height < 0
    ) {
        return;
    }
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    x &= 0xF8;
    image_width &= 0xF8;
    if (x + image_width >= this->width) {
        x_end = this->width - 1;
    } else {
        x_end = x + image_width - 1;
    }
    if (y + image_height >= this->height) {
        y_end = this->height - 1;
    } else {
        y_end = y + image_height - 1;
    }
    SetMemoryArea(x, y, x_end, y_end);
    SetMemoryPointer(x, y);
    SendCommand(0x24);
    /* send the image data */
    for (int j = 0; j < y_end - y + 1; j++) {
        for (int i = 0; i < (x_end - x + 1) / 8; i++) {
            SendData(image_buffer[i + j * (image_width / 8)]);
        }
    }
}
void Epd::SetFrameMemory_Partial(
    const unsigned char* image_buffer,
    int x,
    int y,
    int image_width,
    int image_height
) {
    int x_end;
    int y_end;

    if (
        image_buffer == NULL ||
        x < 0 || image_width < 0 ||
        y < 0 || image_height < 0
    ) {
        return;
    }
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    x &= 0xF8;
    image_width &= 0xF8;
    if (x + image_width >= this->width) {
        x_end = this->width - 1;
    } else {
        x_end = x + image_width - 1;
    }
    if (y + image_height >= this->height) {
        y_end = this->height - 1;
    } else {
        y_end = y + image_height - 1;
    }

    DigitalWrite(reset_pin, LOW);
    DelayMs(2);
    DigitalWrite(reset_pin, HIGH);
    DelayMs(2);
	
	SetLut(_WF_PARTIAL_2IN9);
	SendCommand(0x37); 
	SendData(0x00);  
	SendData(0x00);  
	SendData(0x00);  
	SendData(0x00); 
	SendData(0x00);  	
	SendData(0x40);  
	SendData(0x00);  
	SendData(0x00);   
	SendData(0x00);  
	SendData(0x00);

	SendCommand(0x3C); //BorderWavefrom
	SendData(0x80);	

	SendCommand(0x22); 
	SendData(0xC0);   
	SendCommand(0x20); 
	WaitUntilIdle();  
	
    SetMemoryArea(x, y, x_end, y_end);
    SetMemoryPointer(x, y);
    SendCommand(0x24);
    /* send the image data */
    for (int j = 0; j < y_end - y + 1; j++) {
        for (int i = 0; i < (x_end - x + 1) / 8; i++) {
            SendData(image_buffer[i + j * (image_width / 8)]);
        }
    }
}

/**
 *  @brief: put an image buffer to the frame memory.
 *          this won't update the display.
 *
 *          Question: When do you use this function instead of 
 *          void SetFrameMemory(
 *              const unsigned char* image_buffer,
 *              int x,
 *              int y,
 *              int image_width,
 *              int image_height
 *          );
 *          Answer: SetFrameMemory with parameters only reads image data
 *          from the RAM but not from the flash in AVR chips (for AVR chips,
 *          you have to use the function pgm_read_byte to read buffers 
 *          from the flash).
 */
void Epd::SetFrameMemory(const unsigned char* image_buffer) {
    SetMemoryArea(0, 0, this->width - 1, this->height - 1);
    SetMemoryPointer(0, 0);
    SendCommand(0x24);
    /* send the image data */
    for (int i = 0; i < this->width / 8 * this->height; i++) {
        SendData(pgm_read_byte(&image_buffer[i]));
    }
}
void Epd::SetFrameMemory_Base(const unsigned char* image_buffer) {
    SetMemoryArea(0, 0, this->width - 1, this->height - 1);
    SetMemoryPointer(0, 0);
    SendCommand(0x24);
    /* send the image data */
    for (int i = 0; i < this->width / 8 * this->height; i++) {
        SendData(pgm_read_byte(&image_buffer[i]));
    }
    SendCommand(0x26);
    /* send the image data */
    for (int i = 0; i < this->width / 8 * this->height; i++) {
        SendData(pgm_read_byte(&image_buffer[i]));
    }
}

/**
 *  @brief: clear the frame memory with the specified color.
 *          this won't update the display.
 */
void Epd::ClearFrameMemory(unsigned char color) {
    SetMemoryArea(0, 0, this->width - 1, this->height - 1);
    SetMemoryPointer(0, 0);
    SendCommand(0x24);
    /* send the color data */
    for (int i = 0; i < this->width / 8 * this->height; i++) {
        SendData(color);
    }

    SendCommand(0x26);
    /* send the color data */
    for (int i = 0; i < this->width / 8 * this->height; i++) {
        SendData(color);
    }
}

void Epd::Display4Gray(const unsigned char *Image)
{
    int i,j,k;
    unsigned char temp1,temp2,temp3;

    SendCommand(0x24);	       
    for(i=0;i<4736;i++)	     
    {
        temp3=0;
        for(j=0;j<2;j++)	
        {
            temp1 = pgm_read_byte(&Image[i*2+j]);
            for(k=0;k<2;k++)	
            {
                temp2 = temp1&0xC0 ;
                if(temp2 == 0xC0)
                    temp3 |= 0x00;//white
                else if(temp2 == 0x00)
                    temp3 |= 0x01;  //black
                else if(temp2 == 0x80) 
                    temp3 |= 0x01;  //gray1
                else //0x40
                    temp3 |= 0x00; //gray2
                temp3 <<= 1;	
                
                temp1 <<= 2;
                temp2 = temp1&0xC0 ;
                if(temp2 == 0xC0)  //white
                    temp3 |= 0x00;
                else if(temp2 == 0x00) //black
                    temp3 |= 0x01;
                else if(temp2 == 0x80)
                    temp3 |= 0x01; //gray1
                else    //0x40
                        temp3 |= 0x00;	//gray2	
                if(j!=1 || k!=1)				
                    temp3 <<= 1;
                
                temp1 <<= 2;
            }
            
         }
        SendData(temp3);			
    }
    // new  data
    SendCommand(0x26);	       
    for(i=0;i<4736;i++)	  
    {
        temp3=0;
        for(j=0;j<2;j++)	
        {
            temp1 = pgm_read_byte(&Image[i*2+j]);
            for(k=0;k<2;k++)	
            {
                temp2 = temp1&0xC0 ;
                if(temp2 == 0xC0)
                    temp3 |= 0x00;//white
                else if(temp2 == 0x00)
                    temp3 |= 0x01;  //black
                else if(temp2 == 0x80) 
                    temp3 |= 0x00;  //gray1
                else //0x40
                    temp3 |= 0x01; //gray2
                temp3 <<= 1;	
                
                temp1 <<= 2;
                temp2 = temp1&0xC0 ;
                if(temp2 == 0xC0)  //white
                    temp3 |= 0x00;
                else if(temp2 == 0x00) //black
                    temp3 |= 0x01;
                else if(temp2 == 0x80)
                    temp3 |= 0x00; //gray1
                else    //0x40
                    temp3 |= 0x01;	//gray2
                if(j!=1 || k!=1)					
                    temp3 <<= 1;
                
                temp1 <<= 2;
            }
            
         }
        SendData(temp3);	
    }

    DisplayFrame();
}

/**
 *  @brief: update the display
 *          there are 2 memory areas embedded in the e-paper display
 *          but once this function is called,
 *          the the next action of SetFrameMemory or ClearFrame will 
 *          set the other memory area.
 */
bool Epd::DisplayFrame(void) {
    if (!WaitUntilIdle()) return false;
    // Preserve the Waveshare full-update sequence and host LUT.
    SendCommand(0x22);
    SendData(0xc7);
    SendCommand(0x20);

    // LOW alone is not an acknowledgement. Full refresh must first assert BUSY.
    const uint32_t started = millis();
    while (DigitalRead(busy_pin) != HIGH) {
        if ((uint32_t)(millis() - started) >= 250) {
            last_error = EpdError::RefreshNotStarted;
            return false;
        }
        DelayMs(1);
    }
    // A normal update here takes about 2.2s; bound a stuck HIGH to 15s.
    return WaitUntilIdle(15000);
}

void Epd::DisplayFrame_Partial(void) {
    SendCommand(0x22);
    SendData(0x0F);
    SendCommand(0x20);
    WaitUntilIdle();
}

void Epd::SetLut(unsigned char *lut) {       
	unsigned char count;
	SendCommand(0x32);
	for(count=0; count<153; count++) 
		SendData(lut[count]); 
	WaitUntilIdle();
}

void Epd::SetLut_by_host(unsigned char *lut) {
    SetLut((unsigned char *)lut);
	SendCommand(0x3f);
	SendData(*(lut+153));
	SendCommand(0x03);	// gate voltage
	SendData(*(lut+154));
	SendCommand(0x04);	// source voltage
	SendData(*(lut+155));	// VSH
	SendData(*(lut+156));	// VSH2
	SendData(*(lut+157));	// VSL
	SendCommand(0x2c);		// VCOM
	SendData(*(lut+158));
}

/**
 *  @brief: private function to specify the memory area for data R/W
 */
void Epd::SetMemoryArea(int x_start, int y_start, int x_end, int y_end) {
    SendCommand(0x44);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData((x_start >> 3) & 0xFF);
    SendData((x_end >> 3) & 0xFF);
    SendCommand(0x45);
    SendData(y_start & 0xFF);
    SendData((y_start >> 8) & 0xFF);
    SendData(y_end & 0xFF);
    SendData((y_end >> 8) & 0xFF);
}

/**
 *  @brief: private function to specify the start point for data R/W
 */
void Epd::SetMemoryPointer(int x, int y) {
    SendCommand(0x4E);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData((x >> 3) & 0xFF);
    SendCommand(0x4F);
    SendData(y & 0xFF);
    SendData((y >> 8) & 0xFF);
    WaitUntilIdle();
}

/**
 *  @brief: After this command is transmitted, the chip would enter the 
 *          deep-sleep mode to save power. 
 *          The deep sleep mode would return to standby by hardware reset. 
 *          You can use Epd::Init() to awaken
 */
void Epd::Sleep() {
    if (HasError()) return;
    SendCommand(0x10);
    SendData(0x01);
    // WaitUntilIdle();
}

/* END OF FILE */
