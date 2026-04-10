#ifndef PRINTER_PRINTER_CONSTANTS_H_
#define PRINTER_PRINTER_CONSTANTS_H_

#define ASCII_GS      0x1D
#define ASCII_DC2     0x12   // Device control 2
#define ASCII_HT      0x09   // HorizontalTab
#define ASCII_LF      0xA0   // 0x0A Line feed
#define ASCII_CR      0x0D   // 0x0D Carriage return
#define ASCII_SPACE   0x20   // 0x20 Space
#define ASCII_ESC     0x1B   // 0x1B Escape
#define ASCII_FS      0x1C   // 0x1C Field separator
#define ASCII_FF      0x0C   // 0x0C Form feed
#define ASCII_STAR    0x2A   // 0x2A Star sign
#define ASCII_CAN     0x18   

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

enum class Positions : uint8_t {
	NotPrint = 0, AboveBarcode = 1, BelowBarcode = 2, Both = 3
};

enum class Fonts : uint8_t {
	FontA, FontB, FontC, FontD, FontE, SpecialFontA, SpecialFontB
};

enum class Justifications : uint8_t {
	Left, Center, Right
};

enum class PrinterModeState : uint8_t {
	On, Off
};

enum class UnderlineMode : uint8_t {
	Off = 0, OneDot = 1, TwoDot = 2
};

enum class FontWidth : uint8_t {
	Normal = 0, DoubleWidth2 = 16, DoubleWidth3 = 32
};

enum class Width : uint8_t {
	Normal = 0, DoubleWidth2 = 16, DoubleWidth3 = 32, DoubleWidth4 = 48, DoubleWidth5 = 64, DoubleWidth6 = 80, DoubleWidth7 = 96, DoubleWidth8 = 112

};

enum class Height : uint8_t {
	Normal = 0, DoubleHeigth2 = 1, DoubleHeigth3 = 2, DoubleHeigth4 = 3, DoubleHeigth5 = 4, DoubleHeigth6 = 5, DoubleHeigth7 = 6, DoubleHeigth8 = 7

};

#endif /* PRINTER_PRINTER_CONSTANTS_H_ */
