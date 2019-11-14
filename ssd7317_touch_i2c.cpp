//==============================================================================
//
//  CRYSTALFONTZ CFAL64128A0-096B-WC EXAMPLE FIRMWARE
//
//  TOUCH PANEL CONTROLLER INTERFACE FIRMWARE
//
//  Code written for Seeeduino v4.2 set to 3.3v (important!)
//
//  The controller is a Sitronix SSD7317.
//
//  Seeeduino v4.2, an open-source 3.3v capable Arduino clone.
//    https://www.seeedstudio.com/Seeeduino-V4.2-p-2517.html
//    https://github.com/SeeedDocument/SeeeduinoV4/raw/master/resources/Seeeduino_v4.2_sch.pdf
//
//==============================================================================
//
//  2019-10-30 Mark Williams / Crystalfontz
//
//==============================================================================
//This is free and unencumbered software released into the public domain.
//
//Anyone is free to copy, modify, publish, use, compile, sell, or
//distribute this software, either in source code form or as a compiled
//binary, for any purpose, commercial or non-commercial, and by any
//means.
//
//In jurisdictions that recognize copyright laws, the author or authors
//of this software dedicate any and all copyright interest in the
//software to the public domain. We make this dedication for the benefit
//of the public at large and to the detriment of our heirs and
//successors. We intend this dedication to be an overt act of
//relinquishment in perpetuity of all present and future rights to this
//software under copyright law.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
//OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
//ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
//OTHER DEALINGS IN THE SOFTWARE.
//
//For more information, please refer to <http://unlicense.org/>
//==============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "prefs.h"
#include "ssd7317_touch_i2c.h"
#include "ssd7317_touch_fwblob.h"

//enable to output some debugging information over serial/usb
//#define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define LOG_S(x)	Serial.print((x))
#define LOG_LN(x)	Serial.println((x))
#else
#define LOG_S(x)
#define LOG_LN(x)
#endif

//////////////////////////////////////////////////////////

#define SSD7317_BASE_I2C_ADDR		0x5B
#define SOFT_I2C_READ(a)			((a) << 1)
#define SOFT_I2C_WRITE(a)			(((a) << 1) | 0x01)

#define SSD7317_TOUCH_FW_PAGE_SIZE	0x200

//////////////////////////////////////////////////////////

static void SSD7317_Touch_Setup(void);
static void SSD7317_Touch_IRQ(void);
static void SSD7317_Touch_Process(uint8_t *data);
uint16_t SSD7317_TIC_CPU_BurstRead(uint16_t address, uint8_t data[], uint16_t num);

volatile uint16_t SSD7317_Gesture_Data = 0;
volatile bool SSD7317_TIC_UseHardwareI2C = false;
volatile bool SSD7317_TouchData_Waiting = false;

//////////////////////////////////////////////////////////

void SSD7317_Touch_Init(void)
{
	//vars
	SSD7317_TIC_UseHardwareI2C = false;
	SSD7317_TouchData_Waiting = false;

	//pin setup
	LOG_LN("SSD7317_Touch_Init()");
	digitalWrite(SSD7317_TOUCH_SCL, HIGH);
	pinMode(SSD7317_TOUCH_SCL, OUTPUT);
	digitalWrite(SSD7317_TOUCH_SDA, HIGH);
	pinMode(SSD7317_TOUCH_SDA, OUTPUT);
	digitalWrite(SSD7317_TOUCH_CS, HIGH);
	pinMode(SSD7317_TOUCH_CS, OUTPUT);
	digitalWrite(SSD7317_TOUCH_RST, HIGH);
	pinMode(SSD7317_TOUCH_RST, OUTPUT);
	pinMode(SSD7317_TOUCH_IRQ, INPUT_PULLUP);

	//i2c setup
	//we are using software i2c master mode, software
	// so nothing to do here

	//reset
	digitalWrite(SSD7317_TOUCH_RST, LOW);
	delay(10);
	digitalWrite(SSD7317_TOUCH_RST, HIGH);
	delay(10);

	//run setup commands
	SSD7317_Touch_Setup();

	//setup interrupt handler for touches
	attachInterrupt(digitalPinToInterrupt(SSD7317_TOUCH_IRQ), SSD7317_Touch_IRQ, FALLING);

	LOG_LN("SSD7317_Touch_Init() done");
}

void SSD7317_Touch_HWI2C(bool enable)
{
	//Wire.begin(), Wire.end() are done outside of here
	SSD7317_TIC_UseHardwareI2C = enable;
	if (!enable)
	{
		digitalWrite(SSD7317_TOUCH_SCL, HIGH);
		pinMode(SSD7317_TOUCH_SCL, OUTPUT);
		digitalWrite(SSD7317_TOUCH_SDA, HIGH);
		pinMode(SSD7317_TOUCH_SDA, OUTPUT);
	}
}

//////////////////////////////////////////////////////////

static void SSD7317_Touch_IRQ(void)
{
	//flag main app to check data
	//must call SSD7317_Touch_Handle() within 50mS
	SSD7317_TouchData_Waiting = true;
}

static void SSD7317_Touch_Process(uint8_t *data)
{
	//make the two bytes of touch data useable
	if (data[0] != 0xF6)
		return;

	if (data[1] == 0x04)
	{
		if (data[5] == 0x00)
		{
			//incell
			SSD7317_Gesture_Data = (data[1] << 8) | data[2];
		}
		else
			if (data[5] == 0x01)
			{
				//outcell
				SSD7317_Gesture_Data = (data[1] + data[2]) << 8 | (data[3] & 0x07);
			}
	}
	else
		SSD7317_Gesture_Data = (data[1] << 8) | data[2];
}

void SSD7317_Touch_Handle(void)
{
	//touch int has gone high, get touch data via i2c
	uint8_t snl[2];
	uint8_t data[24];
	uint8_t i;

	SSD7317_TouchData_Waiting = false;

	//read touch status 2 bytes first
	SSD7317_TIC_CPU_BurstRead(0x0AF0, snl, 2);
	if (snl[0] == 0x00 || snl[0] == 0xFF)
		return;
	if (snl[0] > sizeof(data) - 1)
		snl[0] = sizeof(data);

	//now read touch data
	SSD7317_TIC_CPU_BurstRead(0x0AF1, data, snl[0]);
	for (i = 0; i < snl[0]; i += 6)
		//6 bytes per touch point
		if (data[i+0] == 0xF6)
			SSD7317_Touch_Process(&(data[i + 0]));
}

//////////////////////////////////////////////////////////

inline void SSD7317_TIC_I2C_Delay()
{
	_NOP(); _NOP();
}

void SSD7317_TIC_Start()
{
	//software i2c start
	SSD7317_TOUCH_SDA_SET;
	SSD7317_TIC_I2C_Delay();
	SSD7317_TOUCH_SCL_SET;
	SSD7317_TIC_I2C_Delay();
	SSD7317_TOUCH_SDA_CLR;
	SSD7317_TIC_I2C_Delay();
	SSD7317_TOUCH_SCL_CLR;
	SSD7317_TIC_I2C_Delay();
}

void SSD7317_TIC_Stop()
{
	//software i2c stop
	SSD7317_TOUCH_SCL_CLR;
	SSD7317_TIC_I2C_Delay();
	SSD7317_TOUCH_SDA_CLR;
	SSD7317_TIC_I2C_Delay();
	SSD7317_TOUCH_SCL_SET;
	SSD7317_TIC_I2C_Delay();
	SSD7317_TOUCH_SDA_SET;
	SSD7317_TIC_I2C_Delay();
}

static inline uint8_t SSD7317_TOUCH_SDA_RD()
{
	//read SDA pin level
	uint8_t ret;
	pinMode(SSD7317_TOUCH_SDA, INPUT);
	ret = digitalRead(SSD7317_TOUCH_SDA);
	pinMode(SSD7317_TOUCH_SDA, OUTPUT);
	return ret;
}

void SSD7317_TIC_nByte_WR(uint8_t data[], uint16_t num)
{
	//software i2c write data
	uint8_t bitPos;
	uint16_t i;
	for (i = 0; i < num; i++)
	{
		//Output 8-bit data, from MSB to LSB
		for (bitPos = 0x80; bitPos >= 0x01; bitPos >>= 1)
		{
			SSD7317_TOUCH_SCL_CLR;
			if ((bitPos & data[i]) > 0)
				SSD7317_TOUCH_SDA_SET;
			else
				SSD7317_TOUCH_SDA_CLR;
			SSD7317_TIC_I2C_Delay();
			SSD7317_TOUCH_SCL_SET;
			SSD7317_TIC_I2C_Delay();
		}
		SSD7317_TOUCH_SCL_CLR;
		SSD7317_TIC_I2C_Delay();
		SSD7317_TOUCH_SCL_SET; // for ACK
		SSD7317_TIC_I2C_Delay();
		SSD7317_TOUCH_SCL_CLR;
	}
}

void SSD7317_TIC_nByte_RD(uint8_t data[], uint16_t num)
{
	//software i2c read data
	uint8_t bitPos;
	uint16_t i;
	for (i = 0; i < num; i++)
	{
		data[i] = 0x00;

		for (bitPos = 0x80; bitPos >= 0x01; bitPos >>= 1)
		{
			SSD7317_TOUCH_SCL_CLR;
			SSD7317_TIC_I2C_Delay();
			if (SSD7317_TOUCH_SDA_RD() == HIGH)
				data[i] |= bitPos;
			SSD7317_TOUCH_SCL_SET;
			SSD7317_TIC_I2C_Delay();
		}
		SSD7317_TOUCH_SCL_CLR;
		SSD7317_TIC_I2C_Delay();

		if (i + 1 == num)
			SSD7317_TOUCH_SDA_SET;	// NAK for last byte
		else
			SSD7317_TOUCH_SDA_CLR;

		// for acking
		SSD7317_TIC_I2C_Delay();
		SSD7317_TOUCH_SCL_SET;
		SSD7317_TIC_I2C_Delay();
		SSD7317_TOUCH_SCL_CLR;
		SSD7317_TOUCH_SDA_SET;
	}
}

uint16_t SSD7317_TIC_BIOS_RegRead()
{
	uint16_t address = 0x0AF0;
	uint8_t addr[2];
	uint8_t data[2];
	uint16_t ret;

	addr[0] = (uint8_t) ((address >> 0) & 0xFF);
	addr[1] = (uint8_t) ((address >> 8) & 0xFF);

	SSD7317_TIC_Start();
	data[0] = SOFT_I2C_READ(SSD7317_BASE_I2C_ADDR);
	SSD7317_TIC_nByte_WR(data, 1);
	SSD7317_TIC_nByte_WR(addr, 2);
	SSD7317_TIC_Stop();

	delayMicroseconds(80);

	SSD7317_TIC_Start();
	data[0] = SOFT_I2C_WRITE(SSD7317_BASE_I2C_ADDR);
	SSD7317_TIC_nByte_WR(data, 1);
	SSD7317_TIC_nByte_RD(data, 2);
	SSD7317_TIC_Stop();

	ret = (data[1] << 8) + data[0];
	return ret;
}

uint16_t SSD7317_TIC_BIOS_RegWrite(uint16_t address)
{
	uint8_t wd8[3];

	wd8[0] = SOFT_I2C_READ(SSD7317_BASE_I2C_ADDR);
	wd8[1] = (uint8_t) ((address >> 0) & 0xFF);
	wd8[2] = (uint8_t) ((address >> 8) & 0xFF);

	SSD7317_TIC_Start();
	SSD7317_TIC_nByte_WR(wd8, 3);
	SSD7317_TIC_Stop();

	return 0;
}

uint16_t SSD7317_TIC_BIOS_BurstWrite(uint16_t address, uint8_t data[], uint16_t num)
{
	uint8_t wd8[3];

	wd8[0] = SOFT_I2C_READ(SSD7317_BASE_I2C_ADDR | 0x04);
	wd8[1] = (uint8_t) ((address >> 0) & 0xFF);
	wd8[2] = (uint8_t) ((address >> 8) & 0xFF);

	SSD7317_TIC_Start();
	SSD7317_TIC_nByte_WR(wd8, 3);
	SSD7317_TIC_nByte_WR(data, num);
	SSD7317_TIC_Stop();

	return 0;
}

uint16_t SSD7317_TIC_BIOS_BurstWrite_PROGMEM(uint16_t address, uint8_t data[], uint16_t num)
{
	//modified to read data from PROGMEM
	uint8_t wd8[3];
	uint16_t i;

	wd8[0] = SOFT_I2C_READ(SSD7317_BASE_I2C_ADDR | 0x04);
	wd8[1] = (uint8_t) ((address >> 0) & 0xFF);
	wd8[2] = (uint8_t) ((address >> 8) & 0xFF);

	SSD7317_TIC_Start();
	SSD7317_TIC_nByte_WR(wd8, 3);
	for (i = 0; i < num; i++)
	{
		wd8[0] = pgm_read_byte_near(data+i);
		SSD7317_TIC_nByte_WR(wd8, 1);
	}
	SSD7317_TIC_Stop();

	return 0;
}

uint16_t SSD7317_TIC_BIOS_BurstRead(uint16_t address, uint8_t data[], uint16_t num)
{
	uint8_t wd8[3];

	wd8[0] = SOFT_I2C_READ(SSD7317_BASE_I2C_ADDR | 0x04);
	wd8[1] = (uint8_t) ((address >> 0) & 0xFF);
	wd8[2] = (uint8_t) ((address >> 8) & 0xFF);

	SSD7317_TIC_Start();
	SSD7317_TIC_nByte_WR(wd8, 3);
	SSD7317_TIC_Stop();

	delayMicroseconds(80);

	wd8[0] = SOFT_I2C_WRITE(SSD7317_BASE_I2C_ADDR | 0x04);
	SSD7317_TIC_Start();
	SSD7317_TIC_nByte_WR(wd8, 1);
	SSD7317_TIC_nByte_RD(data, num);
	SSD7317_TIC_Stop();

	return 0;
}

uint16_t SSD7317_TIC_CPU_BurstRead(uint16_t address, uint8_t data[], uint16_t num)
{
	uint8_t wd8[3];

	if (SSD7317_TIC_UseHardwareI2C)
	{
		//use hardware I2C
		wd8[0] = (uint8_t) ((address >> 0) & 0xFF);
		wd8[1] = (uint8_t) ((address >> 8) & 0xFF);
		Wire.beginTransmission(SSD7317_BASE_I2C_ADDR);
		Wire.write(wd8, 2);
		Wire.endTransmission();

		delayMicroseconds(80);

		Wire.requestFrom(SSD7317_BASE_I2C_ADDR, num);
		while (Wire.available() < (int)num) {}
		Wire.readBytes(data, num);
	}
	else
	{
		//use software I2C
		wd8[0] = SOFT_I2C_READ(SSD7317_BASE_I2C_ADDR);
		wd8[1] = (uint8_t) ((address >> 0) & 0xFF);
		wd8[2] = (uint8_t) ((address >> 8) & 0xFF);
		SSD7317_TIC_Start();
		SSD7317_TIC_nByte_WR(wd8, 3);
		SSD7317_TIC_Stop();

		delayMicroseconds(80);

		wd8[0] = SOFT_I2C_WRITE(SSD7317_BASE_I2C_ADDR);
		SSD7317_TIC_Start();
		SSD7317_TIC_nByte_WR(wd8, 1);
		SSD7317_TIC_nByte_RD(data, num);
		SSD7317_TIC_Stop();
	}

	return 0;
}

uint16_t SSD7317_TIC_CPU_BurstWrite(uint16_t address, uint8_t data[], uint16_t num)
{
	uint8_t wd8[3];

	wd8[0] = SOFT_I2C_READ(SSD7317_BASE_I2C_ADDR);
	wd8[1] = (uint8_t) ((address >> 0) & 0xFF);
	wd8[2] = (uint8_t) ((address >> 8) & 0xFF);

	SSD7317_TIC_Start();
	SSD7317_TIC_nByte_WR(wd8, 3);
	SSD7317_TIC_nByte_WR(data, num);
	SSD7317_TIC_Stop();

	return 0;
}

//////////////////////////////////////////////////////////

static void SSD7317_Touch_Setup(void)
{
	//SSD7317 integrated touch controller init
	//the function of a lot of this is unknown
	//the manufacturer provides an init flow chart, we just follow it

	//software i2c must be used for init as the touch controller sometimes
	//does not set the I2C ACK bit after a data write.
	//hardware i2c can be used after this init process is complete.

	uint8_t wd8[8];
	int ram_code_size;
	int page_num, write_num;
	int i;
	LOG_LN("SSD7317_Touch_Setup()");

	// wait for touch ic's interrupt to go high after reset
	while (SSD7317_TOUCH_IRQ_RD == HIGH) { _NOP(); }
	LOG_LN("SSD7317_Touch_Setup() - Step1");

	// initialize SSD7317
	SSD7317_TIC_BIOS_RegRead();
	LOG_LN("SSD7317_Touch_Setup() - Step2");

	// write PM firmware blob
	ram_code_size = sizeof(SSD7317_PM_Content) / sizeof(SSD7317_PM_Content[0]);
	SSD7317_TIC_BIOS_RegWrite(RAM_BLOCK_PM);
	delayMicroseconds(300);
	for (i = 0; i < 24; i++)
		SSD7317_TIC_BIOS_BurstWrite_PROGMEM(0x0002 * i, (uint8_t*)SSD7317_PM_Content + SSD7317_TOUCH_FW_PAGE_SIZE * i, SSD7317_TOUCH_FW_PAGE_SIZE);
	LOG_LN("SSD7317_Touch_Setup() - Step3");

	//write TM firmware blob
	ram_code_size = sizeof(SSD7317_TM_Content) / sizeof(SSD7317_TM_Content[0]);
	SSD7317_TIC_BIOS_RegWrite(RAM_BLOCK_TM);
	delayMicroseconds(300);
	page_num = (ram_code_size % SSD7317_TOUCH_FW_PAGE_SIZE) ? (ram_code_size / SSD7317_TOUCH_FW_PAGE_SIZE) + 1 : ram_code_size / SSD7317_TOUCH_FW_PAGE_SIZE;
	for (i = 0; i < page_num; i++)
	{
		ram_code_size -= SSD7317_TOUCH_FW_PAGE_SIZE;
		write_num = (ram_code_size >= 0) ? SSD7317_TOUCH_FW_PAGE_SIZE : (ram_code_size + SSD7317_TOUCH_FW_PAGE_SIZE);
		SSD7317_TIC_BIOS_BurstWrite_PROGMEM(0x0002 * i, (uint8_t*)SSD7317_TM_Content + SSD7317_TOUCH_FW_PAGE_SIZE * i, write_num);
	}
	LOG_LN("SSD7317_Touch_Setup() - Step4");

	//write DM firmware blob
	ram_code_size = sizeof(SSD7317_DM_Content) / sizeof(SSD7317_DM_Content[0]);
	SSD7317_TIC_BIOS_RegWrite(RAM_BLOCK_DM);
	delayMicroseconds(300);
	for (i = 0; i < 4; i++)
		SSD7317_TIC_BIOS_BurstWrite_PROGMEM(0x0002 * i, (uint8_t*)SSD7317_DM_Content + SSD7317_TOUCH_FW_PAGE_SIZE * i, SSD7317_TOUCH_FW_PAGE_SIZE);
	LOG_LN("SSD7317_Touch_Setup() - Step5");

	//firware has been written, do some more misc inits
	SSD7317_TIC_BIOS_RegWrite(0x0000);
	delayMicroseconds(200);
	wd8[0] = 0x00; wd8[1] = 0x00;
	SSD7317_TIC_BIOS_BurstWrite(0x8300, wd8, 2);
	delayMicroseconds(200);
	wd8[0] = 0x03; wd8[1] = 0x00;
	SSD7317_TIC_BIOS_BurstWrite(0x8000, wd8, 2);
	delayMicroseconds(200);
	wd8[0] = 0x00; wd8[1] = 0x00;
	SSD7317_TIC_BIOS_BurstWrite(0x8000, wd8, 2);
	LOG_LN("SSD7317_Touch_Setup() - Step6");

    //wait for touch IC's interrupt to go high then low
	//this signals that the firmware blobs have been loaded successfully
	//  we should probably add a timeout here and re-attempt firmware blob
	//  uploading if timed-out
	while(SSD7317_TOUCH_IRQ_RD == LOW)	{ _NOP(); }
    while(SSD7317_TOUCH_IRQ_RD == HIGH)	{ _NOP(); }

	//firmware has been loaded, last inits
	SSD7317_TIC_CPU_BurstRead(0x0AF0, wd8, 2);
	SSD7317_TIC_CPU_BurstRead(0x0AF1, wd8, 6);
	delay(40);
	wd8[0] = 0x00; wd8[1] = 0x00;
	SSD7317_TIC_CPU_BurstWrite(0x0043, wd8, 2);

	//done
	LOG_LN("SSD7317_Touch_Setup() done");
}

//////////////////////////////////////////////////////////
