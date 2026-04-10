#ifndef PASSWORD_H_
#define PASSWORD_H_

#include <string>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

int key(uint64_t code);
string keyVerify(uint64_t code);
uint64_t getCharCodes(string string);
string getPassword();

string getPasswordMaster();
string getUserMaster();

#ifdef __cplusplus
}
#endif


#endif
