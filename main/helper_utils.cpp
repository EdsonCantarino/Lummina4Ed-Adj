#include "include/helper_utils.h"

void str_pad_to(string &str, const size_t num, const char paddingChar = ' ')
{
    if(num > str.size())
        str.insert(0, num - str.size(), paddingChar);
}

vector<string> str_split(string s, string delimiter) {
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	string token;
	vector<string> res;

	while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
		token = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	res.push_back(s.substr(pos_start));
	return res;
}


uint8_t* hexStringToUint8(string hexString) {
    int len = hexString.length();

    // Allocate memory for result
    uint8_t *result = new uint8_t[len / 2];

    // Iterate through input string
    for (int i = 0; i < len; i += 2) {

        // Extract a pair of characters
        string byteString = hexString.substr(i, 2);

        // Convert it to an integer
        uint8_t byte;
        stringstream ss;
        ss << std::hex << byteString;
        ss >> byte;

        // Store the integer in the result array
        result[i / 2] = byte;
    }

    return result;  // Return the result array
}

uint8_t* convertHexStringToUint8Array(string hexString) {
	int len = hexString.length();

	// Allocate memory for result
	uint8_t *result = new uint8_t[len / 2];

	for (size_t i = 0; i < hexString.length(); i += 2) {
		string byteString = hexString.substr(i, 2);
		uint8_t byte = (uint8_t) strtol(byteString.c_str(), NULL, 16);

		result[i] = byte;
	}

	return result;
}
