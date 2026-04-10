#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <string>
#include <sstream>
#include <iomanip>

#include <stdlib.h>
#include <iostream>
#include <vector>
#include "queue.h"

using namespace std;


void str_pad_to(string &str, const size_t num, const char paddingChar);
vector<string> str_split(string s, string delimiter);
uint8_t* hexStringToUint8(string hexString);
uint8_t* convertHexStringToUint8Array(string hexString);

#endif
