#ifndef NVS_API_H_
#define NVS_API_H_

#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include <vector>

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

using namespace std;

class NVSHelper {
public:
	NVSHelper();

	bool begin(string namespaceNvs = "storage");
	bool open();
	void close();

	bool eraseAll(bool forceCommit = true);
	bool erase(string key, bool forceCommit = true);

	bool setInt(string key, uint8_t value, bool forceCommit = true);
	bool setInt(string key, int16_t value, bool forceCommit = true);
	bool setInt(string key, uint16_t value, bool forceCommit = true);
	bool setInt(string key, int32_t value, bool forceCommit = true);
	bool setInt(string key, uint32_t value, bool forceCommit = true);
	bool setInt(string key, int64_t value, bool forceCommit = true);
	bool setInt(string key, uint64_t value, bool forceCommit = true);
	bool setFloat(string key, float value, bool forceCommit = true);
	bool setString(string key, string value, bool forceCommit = true);
	bool setBlob(string key, uint8_t *blob, size_t length, bool forceCommit =
			true);
	bool setBlob(string key, std::vector<uint8_t> &blob,
			bool forceCommit = true);

	int64_t getInt(string key, int64_t default_value = 0); // In case of error, default_value will be returned
	float getFloat(string key, float default_value = 0);

	bool getString(string key, string &res);
	string getString(string key);


	size_t getBlobSize(string key);  /// Returns the size of the stored blob
	bool getBlob(string key, uint8_t *blob, size_t length); /// User should proivde enought memory to store the loaded blob. If length < than required size to store blob, function fails.
	bool getBlob(string key, vector<uint8_t> &blob);
	vector<uint8_t> getBlob(string key); /// Less eficient but more simple in usage implemetation of `getBlob()`

	bool commit();
protected:
	nvs_handle _nvs_handle;
	string _nvs_namespace;
};

extern NVSHelper NVS;

#endif
