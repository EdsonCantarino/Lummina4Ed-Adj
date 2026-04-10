#ifndef THERMAL_PRINTER_H_
#define THERMAL_PRINTER_H_

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "printer_constants.h"

#include "LinkedList.h"

#define A_GS      0x1D
#define A_DC2     0x12   // Device control 2
#define A_HT      0x09   // HorizontalTab
#define A_LF      0xA0   // 0x0A Line feed
#define A_CR      0x0D   // 0x0D Carriage return
#define A_SPACE   0x20   // 0x20 Space
#define A_ESC     0x1B   // 0x1B Escape
#define A_FS      0x1C   // 0x1C Field separator
#define A_FF      0x0C   // 0x0C Form feed
#define A_STAR    0x2A   // 0x2A Star sign

#define FONT_B 			 1        // Font B: 9x17, Standard font A: 12x24
#define DARK_MODE 		(1 << 1)  // anti-white mode, didn't work?
#define UPSIDE_DOWN 	(1 << 2)  // didn't work?, use invert(bool);
#define BOLD 			(1 << 3)
#define DOUBLE_HEIGHT 	(1 << 4)
#define DOUBLE_WIDTH 	(1 << 5)
#define STRIKEOUT 		(1 << 6)  // didn't work?

// from translator
#define CODEPAGE_CP437 0  // USA, European Standard
#define CODEPAGE_KATAKANA 1
#define CODEPAGE_CP850 2     // Multilingual
#define CODEPAGE_CP860 3     // Portugal
#define CODEPAGE_CP863 4     // Canada-French
#define CODEPAGE_CP865 5     // Nordic
#define CODEPAGE_WCP1251 6   // Slavic
#define CODEPAGE_CP866 7     // Slavic 2
#define CODEPAGE_MIK 8       // Slavic/Bulgarian
#define CODEPAGE_CP755 9     // East Europe, Latvia 2
#define CODEPAGE_IRAN 10     // Persia
#define CODEPAGE_CP862 15    // Hebrew
#define CODEPAGE_WCP1252 16  // Latin 1
#define CODEPAGE_WCP1253 17  // Greece
#define CODEPAGE_CP852 18    // Latin 2
#define CODEPAGE_CP858 19    // Multilingual Latin 1 + ?
#define CODEPAGE_IRAN2 20    // Perisan
#define CODEPAGE_LATVIA 21
#define CODEPAGE_CP864 22       // Arabic
#define CODEPAGE_ISO_8859_1 23  // Western Europe
#define CODEPAGE_CP737 24       // Greece
#define CODEPAGE_WCP1257 25     // Baltic
#define CODEPAGE_THAI 26
#define CODEPAGE_CP720 27  // Arabic
#define CODEPAGE_CP855 28
#define CODEPAGE_CP857 29    // Turkish
#define CODEPAGE_WCP1250 30  // Central Europe
#define CODEPAGE_CP775 31
#define CODEPAGE_WCP1254 32      // Turkish
#define CODEPAGE_WCP1255 33      // Hebrew
#define CODEPAGE_WCP1256 34      // Arabic
#define CODEPAGE_WCP1258 35      // Vietnamese
#define CODEPAGE_ISO_8859_2 36   // Latin 2
#define CODEPAGE_ISO_8859_3 37   // Latin 3
#define CODEPAGE_ISO_8859_4 38   // Baltic
#define CODEPAGE_ISO_8859_5 39   // Slavic
#define CODEPAGE_ISO_8859_6 40   // Arabic
#define CODEPAGE_ISO_8859_7 41   // Greek
#define CODEPAGE_ISO_8859_8 42   // Hebrew
#define CODEPAGE_ISO_8859_9 43   // Turkish
#define CODEPAGE_ISO_8859_15 44  // Latin 9
#define CODEPAGE_THAI2 45
#define CODEPAGE_CP856 46
#define CODEPAGE_CP874 47

#define CHARSET_USA 0
#define CHARSET_FRANCE 1
#define CHARSET_GERMANY 2
#define CHARSET_UK 3
#define CHARSET_DENMARK1 4
#define CHARSET_SWEDEN 5
#define CHARSET_ITALY 6
#define CHARSET_SPAIN_1 7
#define CHARSET_JAPAN 8
#define CHARSET_NORWAY 9
#define CHARSET_DENMARK_2 10
#define CHARSET_SPAIN2 11
#define CHARSET_LATIN_AMERICA 12
#define CHARSET_SOUTH_KOREA 13
#define CHARSET_SLOVENIA 14
#define CHARSET_CHINA 15

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

using namespace std;

class TPrinter {
private:
	int colsNomal = 32;
	int colsCondensed = 42;
	int colsExpanded = 16;

	bool calculateMode { true };

	const uint16_t widthInDots = { 384 }; // to calculate aboslute position, when using some functions like setCharSpacing
	const uint8_t printerBufferLimit = { 255 };  // bytes

	uint16_t cursor { };       // actual position, n of 384
	uint16_t tabs[32] = { }; // default in printer: 8,16,24,32 (*12dots) - doesn't work in my case
	uint8_t tabsAmount = { 0 };
	uint8_t widthMax { 32 };        // max chars per line
	uint8_t charHeight { 24 };      // dots
	uint8_t charWidth { 12 };       // dots
	uint8_t interlineHeight { 6 };  // dots
	uint8_t charSpacing { };       // dots
	uint8_t printMode { };
	uint8_t heating_dots { 9 };
	uint8_t heating_time { 80 };
	uint8_t heating_interval { 2 };

	unsigned long endPoint { };
	unsigned long oneDotHeight_printTime { 40000 };
	unsigned long oneDotHeight_feedTime { 3000 };

	void update();
	void initBitmapData(uint8_t rowsInPackage, uint8_t bytesPerRow);
	void sendBitmapByte(uint8_t byteToSend), setDelayBitmap(uint16_t width,
			uint16_t height, uint16_t blackPixels);

	esp_err_t write_cmd(uint8_t data);

	LinkedList<uint8_t> print_buffer = LinkedList<uint8_t>();

	template<typename T, size_t N>
	size_t countof(T *array);

	template<typename T, size_t N>
	size_t countof(T (&array)[N]);

	void BarCodeTextFont(Fonts f);
	void BarCodeTextPosition(Positions p);
	void BarCodeSize(uint8_t width, uint8_t heigth);

	uint8_t ASCII_CP860(char c);
	void encodingToCP860(char data);
	void encodingToCP860(char *data);



public:
	TPrinter(void);

	void begin();

	void addLF();
	void addCRLF();

	void alignment(Justifications align);
	void Bold(const char *value);
	void Bold(PrinterModeState state);
	void Underline(const char *value);
	void Underline(UnderlineMode state);
	void UnderlineDoublee(const char *value);
	void Expanded(const char *value);
	void Expanded(PrinterModeState state);
	void Condensed(const char *value);
	void Condensed(PrinterModeState state);
	void Font(const char *value, Fonts state);
	void Font(Fonts state);

	void Separator(char speratorChar);
	void Separator(int totalChars, char speratorChar);

	void Code128(uint8_t *code, Positions printString);
	void Code39(uint8_t *code, Positions printString);
	void EAN13(uint8_t *code, Positions printString);

	void TextSize(FontWidth w);
	void TextSize(uint8_t n);

	void TextSize(Width w, Height h, const char * data);

	void testPage();

	void feed();
	void feedPaper(uint8_t n = 1);
	void feedLine(int n = 1);

	void reverseFeed(uint8_t n);

	void setCharSize(uint8_t n);

	void setCodePage(uint8_t page = 36);
	void setCharset(uint8_t val = 14);

	// true - ON;
	// calculate on the basis of heating points, time and interval
	// every time when you change printMode or heating parameters

	// false - off
	// calculate on the basis of oneDotHeight_printTime and oneDotHeight_feedTime
	// you can change values above using setTimes
	void autoCalculate(bool val = true);

	void calculatePrintTime();
	void setTimes(unsigned long p = 30000, unsigned long f = 3000);
	void setHeat(uint8_t n1 = 0, uint8_t n2 = 255, uint8_t n3 = 0);

	// best quality: the smallest number of dots burned, the longest heating time
	// dots: default 9 (80dots) becouse (9+1)* 8; units: 8 dots;
	// time: default 80 (800 us); units: 10 us
	// interval: default 2 (20us); units :10 us

	void setMode(uint8_t = 0, uint8_t = 0, uint8_t = 0, uint8_t = 0,
			uint8_t = 0, uint8_t = 0, uint8_t = 0), unsetMode(uint8_t = 0,
			uint8_t = 0, uint8_t = 0, uint8_t = 0, uint8_t = 0, uint8_t = 0,
			uint8_t = 0);

	void invert(bool n = 0);	// 1 - invert ON
	void justify(char val);	// 'L' - Left 'C' - center 'R' - right
	void underline(uint8_t n);	// off 0 - 2 max
	void setInterline(uint8_t n);	// default: 6,after call begin() set to 0
	void setCharSpacing(uint8_t n = 0);	// n x 0.125 millimeters(1dot); x2 if double width
	void setTabs(uint8_t *tab = 0, uint8_t size = 0);
	void clearTabs();
	void tab();

	void reset();

	void offline();
	void online();

	void newLine();

	void printBitmap(uint8_t *bitmap, uint16_t width, uint16_t height,
			uint8_t scale = 1, bool center = true);

	void append(char data);
	void append(char data, bool newLine);
	void append(const char *data);
	void append(const char *data, bool newLine);
	void appendByte(uint8_t data);
	void appendBytes(uint8_t data[]);
	void appendByte(uint8_t data, bool newLine);
	void appendBytes(uint8_t data[], bool newLine);
	void appendBytesHexString(string hexString);
	void appendBytesHexString(string hexString, bool newLine);

	void encoding(char data);
	void encoding(char *data);

	void clear();

	void end();

	uint8_t* print();
	size_t size();

	void sleep();
	void sleepAfter(uint16_t seconds);

	void normalSize(const char *data);
	void bold(const char *data);
	void boldMedium(const char *data);
	void boldLarge(const char *data);

};

#endif

