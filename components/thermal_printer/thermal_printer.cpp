#include <stdio.h>
#include <string.h>
#include "thermal_printer.h"

TPrinter::TPrinter() { // @suppress("Class members should be properly initialized")

}

void TPrinter::append(char data) {
	print_buffer.add(data);
}

void TPrinter::append(char data, bool newline) {
	append(data);

	if (newline)
		newLine();
}

void TPrinter::append(const char *data) {
	for (int i = 0; i < strlen(data); i++) {
		print_buffer.add(data[i]);
	}
}

void TPrinter::append(const char *data, bool newline) {
	append(data);

	if (newline)
		newLine();
}

void TPrinter::appendByte(uint8_t data) {
	print_buffer.add(data);
}

void TPrinter::appendByte(uint8_t data, bool newline) {
	appendByte(data);

	if (newline)
		newLine();
}

void TPrinter::appendBytesHexString(string hexString) {
	for (size_t i = 0; i < hexString.length(); i += 2) {
		string byteString = hexString.substr(i, 2);
		uint8_t byte = (uint8_t) strtol(byteString.c_str(), NULL, 16);

		appendByte(byte);
	}
}

void TPrinter::appendBytesHexString(string hexString, bool newline) {
	appendBytesHexString(hexString);

	if (newline)
		newLine();
}

void TPrinter::appendBytes(uint8_t data[]) {

	int size = sizeof(data) / sizeof(data[0]);

	for (int i = 0; i < size; i++) {
		print_buffer.add(data[i]);
	}
}

void TPrinter::appendBytes(uint8_t data[], bool newline) {
	appendBytes(data);

	if (newline) {
		newLine();
	}
}

void TPrinter::addLF() {
	newLine();
}

void TPrinter::addCRLF() {
	append(A_CR);
	newLine();
}

void TPrinter::alignment(Justifications align) {
	print_buffer.add(ASCII_ESC);
	print_buffer.add(0x61);
	print_buffer.add((uint8_t) align);
}

void TPrinter::Bold(const char *value) {
	Bold(PrinterModeState::On);
	append(value);
	Bold(PrinterModeState::Off);
}

void TPrinter::Bold(PrinterModeState state) {
	print_buffer.add(ASCII_ESC);
	print_buffer.add(0x45);
	print_buffer.add(state == PrinterModeState::On ? 0x01 : 0x00);
}

void TPrinter::Underline(const char *value) {
	Underline(UnderlineMode::OneDot);
	append(value);
	Underline(UnderlineMode::Off);
	addLF();
}

void TPrinter::UnderlineDoublee(const char *value) {
	Underline(UnderlineMode::TwoDot);
	append(value);
	Underline(UnderlineMode::Off);
	addLF();
}

void TPrinter::Underline(UnderlineMode state) {
	print_buffer.add(ASCII_ESC);
	print_buffer.add(0x2D);
	print_buffer.add((uint8_t) state);
}

void TPrinter::Expanded(const char *value) {
	Expanded(PrinterModeState::On);
	append(value);
	Expanded(PrinterModeState::Off);
	addLF();
}

void TPrinter::Expanded(PrinterModeState state) {
	print_buffer.add(ASCII_GS);
	print_buffer.add('!');
	print_buffer.add(state == PrinterModeState::On ? 1 : 0);
}

void TPrinter::Condensed(const char *value) {
	Condensed(PrinterModeState::On);
	append(value);
	Condensed(PrinterModeState::Off);
	addLF();
}

void TPrinter::Condensed(PrinterModeState state) {
	print_buffer.add(ASCII_ESC);
	print_buffer.add('!');
	print_buffer.add(state == PrinterModeState::On ? 1 : 0);
}

void TPrinter::Separator(int totalChars, char speratorChar) {
	Condensed(PrinterModeState::On);

	for (int i = 0; i < totalChars - 1; i++) {
		append(speratorChar);
	}

	Condensed(PrinterModeState::Off);
}

void TPrinter::Separator(char speratorChar) {
	justify('L');
	Separator(colsCondensed, speratorChar);
	newLine();
}

void TPrinter::Font(const char *value, Fonts state) {
	Font(state);
	append(value);
	Font(Fonts::FontA);
	addLF();
}

void TPrinter::Font(Fonts state) {
	uint8_t fnt = 0;

	switch (state) {
	case Fonts::FontA: {
		fnt = 0;
		break;
	}

	case Fonts::FontB: {
		fnt = 1;
		break;
	}

	case Fonts::FontC: {
		fnt = 2;
		break;
	}

	case Fonts::FontD: {
		fnt = 3;
		break;
	}

	case Fonts::FontE: {
		fnt = 4;
		break;
	}

	case Fonts::SpecialFontA: {
		fnt = 5;
		break;
	}

	case Fonts::SpecialFontB: {
		fnt = 6;
		break;
	}
	default:
		fnt = 0;
		break;
	}

	print_buffer.add(ASCII_ESC);
	print_buffer.add(0x4D);
	print_buffer.add(fnt);
}

void TPrinter::clear() {
	print_buffer.clear();

	append(A_ESC);
	append(0x40);
}

void TPrinter::TextSize(FontWidth w) {
	if (w == FontWidth::Normal) {
		appendByte(ASCII_ESC);
	} else {
		appendByte(ASCII_GS);
	}

	appendByte(0x21);
	appendByte((uint8_t) w);
}

void TPrinter::TextSize(uint8_t n) {
	appendByte(ASCII_GS);
	appendByte(0x21);
	appendByte(n);
}

// private
void TPrinter::update() {
	charHeight = (printMode & FONT_B) ? 17 : 24;  // B : A
	charWidth = (printMode & FONT_B) ? 9 : 12;
	widthMax = (printMode & FONT_B) ? 42 : 32;
	if (printMode & DOUBLE_WIDTH) {
		charWidth *= 2;
		widthMax /= 2;
	}
	if (printMode & DOUBLE_HEIGHT)
		charHeight *= 2;
}

void TPrinter::feed() {
	append(A_LF);
	cursor = 0;
}

void TPrinter::reverseFeed(uint8_t n) {
	append(A_ESC);
	append(0x4B);
	append(n);

	cursor = 0;
}

void TPrinter::feedPaper(uint8_t n) {
	append(A_ESC);
	append(0x4A);
	append(n);

	cursor = 0;
}

void TPrinter::feedLine(int n) {
	for(int i = 0; i < n; i++){
		append(A_ESC);
		append(0x0A); 
	}

	cursor = 0;
}

void TPrinter::tab() {
// If you don't set the next horizontal tab position
// (bigger than actual cursor position),
// the command is ignored.
	for (uint8_t i = 0; i < tabsAmount; i++) {
		if (tabs[i] > cursor) {
			cursor = tabs[i];
			break;
		}
	}

	append(A_HT);

	if ((widthInDots - cursor) < charWidth) {
		cursor = 0;  // printer go newline
	}
}

void TPrinter::clearTabs() {
	append(A_ESC);
	append('D');
	append('0');

	tabs[tabsAmount = 0] = 0;
}

void TPrinter::setCharSize(uint8_t n) {
	append(A_GS);
	append(0x21);
	append(n);
}

void TPrinter::setCodePage(uint8_t page) {
	append(A_ESC);
	append(0x74);
	append(page);
}

void TPrinter::setCharset(uint8_t val) {
	if (val > 15)
		val = 15;

	append(A_ESC);
	append('R');
	append(val);
}

void TPrinter::setHeat(uint8_t n1, uint8_t n2, uint8_t n3) {
	append(A_ESC);
	append('7');
	append(heating_dots = n1);
	append(heating_time = n2);
	append(heating_interval = n3);
}

void TPrinter::setMode(uint8_t m1, uint8_t m2, uint8_t m3, uint8_t m4,
		uint8_t m5, uint8_t m6, uint8_t m7) {

	printMode |= (m1 + m2 + m3 + m4 + m5 + m6 + m7);
	append(A_ESC);
	append('!');
	append(printMode);

	update();
}

void TPrinter::unsetMode(uint8_t m1, uint8_t m2, uint8_t m3, uint8_t m4,
		uint8_t m5, uint8_t m6, uint8_t m7) {

	printMode &= ~(m1 + m2 + m3 + m4 + m5 + m6 + m7);
	append(A_ESC);
	append('!');
	append(printMode);

	update();
}

void TPrinter::invert(bool n) {
	append(A_ESC);
	append('{');
	append(n);
}

void TPrinter::justify(char val) {
	append(A_ESC);
	append(0x61);

	switch (val) {
	case 'L':
		appendByte(0x00);
		break;
	case 'C':
		appendByte(0x01);
		break;
	case 'R':
		appendByte(0x02);
		break;
	}
}

void TPrinter::underline(uint8_t n) {
	if (n > 2)
		n = 2;

	append(A_ESC);
	append('-');
	append(n);
}

void TPrinter::setInterline(uint8_t n) {
// ESC '2' - back to default

	append(A_ESC);
	append('3');

	if (n + charHeight >= 255) {
		interlineHeight = 255;
	} else {
		interlineHeight = n + charHeight;
	}

	append(interlineHeight);
	interlineHeight -= charHeight;

	update();
}

void TPrinter::setCharSpacing(uint8_t n) {
// printer default: 0
	append(A_ESC);
	append(A_SPACE);
	append(charSpacing = n);
}

void TPrinter::setTabs(uint8_t *tab, uint8_t size) {
// Enter values â€‹â€‹in ascending order.
// The next value can't be equal to or less than the previous one
// - the printer will consider it as a data
// (will finish setting the tabs)
// sets as absolute position;
	tabs[tabsAmount = 0] = 0;

	append(A_ESC);
	append('D');

	for (uint8_t i = 0; i < size; i++) {
		if (tab[i] < widthMax && tab[i] > (tabs[tabsAmount] / charWidth)
				&& tabsAmount < 32) {
			append(tab[i]);
			tabs[tabsAmount++] = tab[i] * charWidth;
		}
	}
	appendByte(0x00);  // from datasheet - end of list
	cursor = 0;
}

void TPrinter::reset() {
	append(A_ESC);
	append('@');

	cursor = 0;
	tabs[tabsAmount = 0] = 0;
	interlineHeight = 6;
	printMode = 0;
	charSpacing = 0;
	heating_dots = 9;
	heating_time = 80;
	heating_interval = 2;
	update();
}

//void TPrinter::testPage() {
//	append(A_GS);
//	append('(');
//	append('A');
//	appendByte(0x02);
//	appendByte(0x00);
//	appendByte(0x00);
//	appendByte(0x03);
//}

void TPrinter::begin() {
	reset();
	online();
	setHeat();
	setCodePage(0x03);
	setCharset();
	setInterline(0);  // save paper during testing
	uint8_t list[] = { 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48 };
	setTabs(list, 12);  // if widthMax == 32, last tab is 28
}

void TPrinter::offline() {
	append(A_ESC);
	append('=');
	appendByte(0x00);
}

void TPrinter::online() {
	append(A_ESC);
	append('=');
	appendByte(0x01);
}

void TPrinter::newLine() {
	appendByte(0x0A);
}

uint8_t* TPrinter::print() {

	int size = print_buffer.size();

	uint8_t buffer[size];

	for (int i = 0; i < size; i++) {
		uint8_t d = (uint8_t) print_buffer.get(i);
		buffer[i] = d;
	}

	uint8_t *b = buffer;

	return b;
}

size_t TPrinter::size() {
	return print_buffer.size();
}

void TPrinter::end() {
	appendByte(0x00);
}

void TPrinter::TextSize(Width w, Height h, const char *data) {
	appendByte(ASCII_GS);
	
	appendByte(0x21);
	appendByte((uint8_t) ((int) w) + ((int) h));
	append(data);
	newLine();

	appendByte(ASCII_GS);
	appendByte(0x21);
	appendByte((uint8_t) ((int) Width::Normal) + ((int) Height::Normal));
}

void TPrinter::normalSize(const char *data) {
	appendByte(ASCII_GS);
	appendByte(0x21);
	appendByte((uint8_t) ((int) Width::Normal) + ((int) Height::Normal));
	append(data);
	newLine();
}

void TPrinter::bold(const char *data) {
	//  Set unidirectional print mode: Cancel unidirectional print mode
	//  (Unnecessary in the TM-T20, so it is comment out)
	//  ESC "U" 0
	//  Select character size: Normal size
	append(ASCII_GS);
	append('!');
	appendByte(0x00);

	append(A_ESC);
	append(0x21);
	append(0x08);
	append(data);
}

void TPrinter::boldMedium(const char *data) {
	append(A_ESC);
	append(0x21);
	append(0x20);
	append(data);
}

void TPrinter::boldLarge(const char *data) {
	append(A_ESC);
	append(0x21);
	append(0x10);
	append(data);
}

void TPrinter::testPage() {
	//  ============================================================================
	//  Issuing receipts
	//  ============================================================================

	//  Initialize printer
	append(ASCII_ESC);
	append('@');

	//  --- Print stamp --->>>
	//  Set line spacing: For the TM-T20, 1.13 mm (18/406 inches)
	append(ASCII_ESC);
	append('3');
	append(18);

	//  Set unidirectional print mode: Cancel unidirectional print mode (Unnecessary
	//  in the TM-T20, so it is comment out)
	//  For models equipped with the ESC U command, implementation is recommended.
	//  Designate unidirectional printing with the ESC U command to get a print
	//  result with equal upper and lower parts for the stamp frame symbol
	//  ESC "U" 1
	//  Select justification: Centering
	append(ASCII_ESC);
	append('a');
	append(1);

	//  Select character size: (horizontal (times 2) x vertical (times 2))
	append(ASCII_GS);
	append('!');
	append(0x11);

	//  Print stamp data and line feed: quadruple-size character section, 1st line
	append(0xC9);
	append(0xCD);

	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xBB);
	newLine();

	//  Print stamp data and line feed: quadruple-size character section, 2nd line

	append(0xBA);
	append(0x20);
	append(0x20);
	append(0x20);
	append(0x45);
	append(0x50);
	append(0x53);
	append(0x4F);
	append(0x4E);
	append(0x20);
	append(0x20);
	append(0x20);
	append(0xBA);
	newLine();

	//  Print stamp data and line feed: quadruple-size character section, 3rd line
	//  Left frame and empty space data
	append(0xBA);
	append(0x20);
	append(0x20);
	append(0x20);

	//  Select character size: Normal size
	append(ASCII_GS);
	append('!');
	appendByte(0x00);

	//  Character string data in the frame
	append("Thank you ");

	//  Select character size: horizontal (times 2) x vertical (times 1)
	append(ASCII_GS);
	append('!');
	append(0x11);

	//  Empty space and right frame data, and print and line feed
	append(0x20);
	append(0x20);
	append(0x20);
	append(0xBA);
	newLine();

	//  Print stamp data and line feed: quadruple-size character section, 4th line
	append(0xC8);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xCD);
	append(0xBC);
	newLine();

	//  Initializing line spacing
	append(ASCII_ESC);
	append('2');

	//  Set unidirectional print mode: Cancel unidirectional print mode
	//  (Unnecessary in the TM-T20, so it is comment out)
	//  ESC "U" 0
	//  Select character size: Normal size
	append(ASCII_GS);
	append('!');
	appendByte(0x00);

	//  --- Print stamp ---<<<

	//  --- Print the date and time --->>>
	//  Print and feed paper: In case TM-T20, feeding amount = 0.250 mm (4/406 inches)
	append(ASCII_ESC);
	append('J');
	append(4);
	append("NOVEMBER 1, 2012  10:30");

	//  Print and feed n lines: Feed the paper three lines
	append(ASCII_ESC);
	append('d');
	append(3);

	//  --- Print the date and time ---<<<

	//  --- Print details A --->>>
	//  Select justification: Left justification
	append(ASCII_ESC);
	append('a');
	appendByte(0);

	//  Details text data and print and line feed
	append("TM-Uxxx                     6.75");
	newLine();
	append("TM-Hxxx                     6.00");
	newLine();
	append("PS-xxx                      1.70");
	newLine();
	newLine();

	//  --- Print details A ---<<<

	//  --- Print details B --->>>
	//  Set unidirectional print mode: Set unidirectional print mode
	//  (Unnecessary in the TM-T20, so it is comment out)
	//  For models equipped with the ESC U command, implementation is recommended.
	//  Designate unidirectional printing with the ESC U command to get a print
	//  result with equal upper and lower parts for double-height characters
	//  ESC "U" 1
	//  Select character size: horizontal (times 1) x vertical (times 2)
	append(ASCII_GS);
	append('!');
	append(0x01);

	//  Details text data and print and line feed
	append("TOTAL                     14.45");
	newLine();

	//  Set unidirectional print mode: Cancel unidirectional print mode
	//  (Unnecessary in the TM-T20, so it is comment out)
	//  ESC "U" 0
	//  Select character size: Normal size
	append(ASCII_GS);
	append('!');
	appendByte(0x00);

	//  Details characters data and print and line feed
	append("--------------------------------");
	newLine();
	append("PAID                       50.00");
	newLine();
	append("CHANGE                     35.55");
	newLine();

	//  --- Print details B ---<<<

	//  --- Issue receipt --->>>
	//  Operating the drawer
	//  Generate pulse: Drawer kick-out connector pin 2, 2 x 2 ms on, 20 x 2 ms off
	append(ASCII_ESC);
	append('p');
	appendByte(0);
	append(2);
	append(20);

	//  Select cut mode and cut paper: [Function B] Feed paper to (cutting position
	//  + 0 mm) and executes a partial cut (one point left uncut).
	append(ASCII_GS);
	append('V');
	append(66);
	appendByte(0);

	//  --- Issue receipt ---<<<
	//  ============================================================================
	//  Issuing receipts
	//  ============================================================================

//	//  Initialize printer
//	append(ASCII_ESC);
//	append('@');
//
//	//  --- Print stamp --->>>
//	//  Set line spacing: For the TM-T20, 1.13 mm (18/406 inches)
//	append(ASCII_ESC);
//	append('3');
//	append(18);
//	//  Set unidirectional print mode: Cancel unidirectional print mode (Unnecessary
//	//  in the TM-T20, so it is comment out)
//	//  For models equipped with the ESC U command, implementation is recommended.
//	//  Designate unidirectional printing with the ESC U command to get a print
//	//  result with equal upper and lower parts for the stamp frame symbol
//	//  ESC "U" 1
//	//  Select justification: Centering
//	append(ASCII_ESC);
//	append('a');
//	append(1);
//
//	//  Select character size: (horizontal (times 2) x vertical (times 2))
//	append(ASCII_GS);
//	append('!');
//	append(0x11);
//
//	//  Print stamp data and line feed: quadruple-size character section, 1st line
//	append(0xC9);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xBB);
//	newLine();
//
//	//  Print stamp data and line feed: quadruple-size character section, 2nd line
//	append(0xBA);
//	append(0x20);
//	append(0x20);
//	append(0x20);
//	append(0x45);
//	append(0x50);
//	append(0x53);
//	append(0x4F);
//	append(0x4E);
//	append(0x20);
//	append(0x20);
//	append(0x20);
//	append(0xBA);
//	newLine();
//
//	//  Print stamp data and line feed: quadruple-size character section, 3rd line
//	//  Left frame and empty space data
//	append(0xBA);
//	append(0x20);
//	append(0x20);
//	append(0x20);
//
//	//  Select character size: Normal size
//	append(ASCII_GS);
//	append('!');
//	appendByte(0x00);
//
//	//  Character string data in the frame
//	append("Thank you ");
//
//	//  Select character size: horizontal (times 2) x vertical (times 1)
//	append(ASCII_GS);
//	append('!');
//	append(0x11);
//
//	//  Empty space and right frame data, and print and line feed
//	append(0x20);
//	append(0x20);
//	append(0x20);
//	append(0xBA);
//	newLine();
//
//	//  Print stamp data and line feed: quadruple-size character section, 4th line
//	append(0xC8);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xCD);
//	append(0xBC);
//	newLine();
//
//	//  Initializing line spacing
//	append(ASCII_ESC);
//	append('2');
//
//	//  Set unidirectional print mode: Cancel unidirectional print mode
//	//  (Unnecessary in the TM-T20, so it is comment out)
//	//  ESC "U" 0
//	//  Select character size: Normal size
//	append(ASCII_GS);
//	append('!');
//	appendByte(0x00);
//
//	//  --- Print stamp ---<<<
//
//	//  --- Print the date and time --->>>
//	//  Print and feed paper: In case TM-T20, feeding amount = 0.250 mm (4/406 inches)
//	append(ASCII_ESC);
//	append('J');
//	append(4);
//	append("NOVEMBER 1, 2012  10:30");
//
//	//  Print and feed n lines: Feed the paper three lines
//	append(ASCII_ESC);
//	append('d');
//	append(3);
//
//	//  --- Print the date and time ---<<<
//
//	//  --- Print details A --->>>
//	//  Select justification: Left justification
//	append(ASCII_ESC);
//	append('a');
//	appendByte(0);
//
//	//  Details text data and print and line feed
//	append("TM-Uxxx                            6.75");
//	newLine();
//	append("TM-Hxxx                            6.00");
//	newLine();
//	append("PS-xxx                             1.70");
//	newLine();
//	newLine();
//
//	//  --- Print details A ---<<<
//
//	//  --- Print details B --->>>
//	//  Set unidirectional print mode: Set unidirectional print mode
//	//  (Unnecessary in the TM-T20, so it is comment out)
//	//  For models equipped with the ESC U command, implementation is recommended.
//	//  Designate unidirectional printing with the ESC U command to get a print
//	//  result with equal upper and lower parts for double-height characters
//	//  ESC "U" 1
//	//  Select character size: horizontal (times 1) x vertical (times 2)
//	append(ASCII_GS);
//	append('!');
//	append(0x01);
//
//	//  Details text data and print and line feed
//	append("TOTAL                             14.45");
//	newLine();
//
//	//  Set unidirectional print mode: Cancel unidirectional print mode
//	//  (Unnecessary in the TM-T20, so it is comment out)
//	//  ESC "U" 0
//	//  Select character size: Normal size
//	append(ASCII_GS);
//	append('!');
//	appendByte(0x00);
//
//	//  Details characters data and print and line feed
//	append("---------------------------------------");
//	newLine();
//	append("PAID                              50.00");
//	newLine();
//	append("CHANGE                            35.55");
//	newLine();
//
//	//  --- Print details B ---<<<
//
//	//  --- Issue receipt --->>>
//	//  Operating the drawer
//	//  Generate pulse: Drawer kick-out connector pin 2, 2 x 2 ms on, 20 x 2 ms off
//	append(ASCII_ESC);
//	append('p');
//	appendByte(0);
//	append(2);
//	append(20);
//
//	//  Select cut mode and cut paper: [Function B] Feed paper to (cutting position
//	//  + 0 mm) and executes a partial cut (one point left uncut).
//	append(ASCII_GS);
//	append('V');
//	appendByte(66);
//	appendByte(0);
//	//  --- Issue receipt ---<<<

}
