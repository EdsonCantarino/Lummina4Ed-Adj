#include "include/security.h"

#include "include/nvs_utils.h"
#include "include/serial_number.h"

const string MASTER_USER = "L4XXBIO";
const string MASTER_PASS = "128500A";

int key(uint64_t code) {
	return (int)((code * 983) % 10000);
}

string keyVerify(uint64_t code) {
	int k = (int)((code * 701 + 307) % 10000);

	return to_string(k).append(4 - to_string(k).length(), '0');
}

uint64_t getCharCodes(string string) {
	uint64_t password = 0;

	for (int i = 0; i < string.length(); i++) {

		int code = string[i];

		password += code;

		password *= 0x16;
	}

	return password;
}

string getPassword() {
	string serialNumber = get_serial_number();

	uint64_t p = getCharCodes(serialNumber);

	int k = key(p);

	string password = keyVerify(k);

	password += password;

	return password;
}

string getPasswordMaster(){
	return MASTER_PASS;
}

string getUserMaster(){
	return MASTER_USER;
}
