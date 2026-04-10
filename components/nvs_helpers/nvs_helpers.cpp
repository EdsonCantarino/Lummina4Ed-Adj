#include <stdio.h>
#include "include/nvs_helpers.h"

NVSHelper::NVSHelper() {
}

bool NVSHelper::begin(string namespaceNvs) {
	esp_err_t err = nvs_flash_init();
	if (err != ESP_OK) {
		ESP_LOGE("NVS_Helper", "Error (%s) init NVS!\n", esp_err_to_name(err));
		if (err != ESP_ERR_NVS_NO_FREE_PAGES)
			return false;

		// erase and reinit
		ESP_LOGI("NVS_Helper", "NVS. Try reinit the partition");
		const esp_partition_t *nvs_partition = esp_partition_find_first(
				ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
		if (nvs_partition == NULL)
			return false;
		err = esp_partition_erase_range(nvs_partition, 0, nvs_partition->size);
		esp_err_t err = nvs_flash_init();
		if (err)
			return false;
		ESP_LOGI("NVS_Helper", "NVS. Partition re-formatted");
	}

	_nvs_namespace = namespaceNvs;

	return true;
}

bool NVSHelper::open() {
	esp_err_t err = nvs_open(_nvs_namespace.c_str(), NVS_READWRITE,
			&_nvs_handle);
	if (err != ESP_OK)
		return false;

	return true;
}

void NVSHelper::close() {
	nvs_close(_nvs_handle);
}

bool NVSHelper::eraseAll(bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_erase_all(_nvs_handle);
	if (err != ESP_OK) {
		close();

		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::erase(string key, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_erase_key(_nvs_handle, key.c_str());

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::commit() {
	esp_err_t err = nvs_commit(_nvs_handle);

	if (err != ESP_OK)
		return false;

	close();

	return true;
}

bool NVSHelper::setInt(string key, uint8_t value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_u8(_nvs_handle, (char*) key.c_str(), value);

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setInt(string key, int16_t value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_i16(_nvs_handle, (char*) key.c_str(), value);

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setInt(string key, uint16_t value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_u16(_nvs_handle, (char*) key.c_str(), value);

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setInt(string key, int32_t value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_i32(_nvs_handle, (char*) key.c_str(), value);

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setInt(string key, uint32_t value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_u32(_nvs_handle, (char*) key.c_str(), value);

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}
bool NVSHelper::setInt(string key, int64_t value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_i64(_nvs_handle, (char*) key.c_str(), value);

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setInt(string key, uint64_t value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_u64(_nvs_handle, (char*) key.c_str(), value);

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setString(string key, string value, bool forceCommit) {
	if (!open()) return false;

	esp_err_t err = nvs_set_str(_nvs_handle, (char*) key.c_str(),
			value.c_str());

	if (err != ESP_OK) {
		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setBlob(string key, uint8_t *blob, size_t length,
		bool forceCommit) {
	if (!open()) return false;

	ESP_LOGI("NVS_Helper",
			"NVSHelper::setObjct(): set obj addr = [0x%X], length = [%d]\n",
			(unsigned int)blob, length);

	if (length == 0)
		return false;

	esp_err_t err = nvs_set_blob(_nvs_handle, (char*) key.c_str(), blob,
			length);

	if (err) {
		ESP_LOGE("NVS_Helper", "NVSHelper::setObjct(): err = [0x%X]\n", (unsigned int)err);

		close();
		return false;
	}

	return forceCommit ? commit() : true;
}

bool NVSHelper::setBlob(string key, std::vector<uint8_t> &blob,
		bool forceCommit) {
	return setBlob(key, &blob[0], blob.size(), forceCommit);
}

int64_t NVSHelper::getInt(string key, int64_t default_value) {
	uint8_t v_u8;
	int16_t v_i16;
	uint16_t v_u16;
	int32_t v_i32;
	uint32_t v_u32;
	int64_t v_i64;
	uint64_t v_u64;

	esp_err_t err;

	if (!open()) return default_value;

	err = nvs_get_u8(_nvs_handle, (char*) key.c_str(), &v_u8);
	if (err == ESP_OK) {
		close();
		return (int64_t) v_u8;
	}

	err = nvs_get_i16(_nvs_handle, (char*) key.c_str(), &v_i16);
	if (err == ESP_OK) {
		close();
		return (int64_t) v_i16;
	}

	err = nvs_get_u16(_nvs_handle, (char*) key.c_str(), &v_u16);
	if (err == ESP_OK) {
		close();
		return (int64_t) v_u16;
	}

	err = nvs_get_i32(_nvs_handle, (char*) key.c_str(), &v_i32);
	if (err == ESP_OK) {
		close();
		return (int64_t) v_i32;
	}

	err = nvs_get_u32(_nvs_handle, (char*) key.c_str(), &v_u32);
	if (err == ESP_OK) {
		close();
		return (int64_t) v_u32;
	}

	err = nvs_get_i64(_nvs_handle, (char*) key.c_str(), &v_i64);
	if (err == ESP_OK) {
		close();
		return (int64_t) v_i64;
	}

	err = nvs_get_u64(_nvs_handle, (char*) key.c_str(), &v_u64);
	if (err == ESP_OK) {
		close();
		return (int64_t) v_u64;
	}

	close();

	return default_value;
}

bool NVSHelper::getString(string key, string &res) {
	size_t required_size;
	esp_err_t err;

	if (!open()) return false;

	err = nvs_get_str(_nvs_handle, key.c_str(), NULL, &required_size);
	if (err) {
		close();
		return false;
	}

	char value[required_size];
	err = nvs_get_str(_nvs_handle, key.c_str(), value, &required_size);
	if (err) {
		close();
		return false;
	}

	res = value;

	close();
	return true;
}

string NVSHelper::getString(string key) {
	string res;
	bool ok = getString(key, res);
	if (!ok)
		return string();
	return res;
}

size_t NVSHelper::getBlobSize(string key) {
	size_t required_size;

	if (!open()) return false;

	esp_err_t err = nvs_get_blob(_nvs_handle, key.c_str(), NULL,
			&required_size);
	if (err) {
		if (err != ESP_ERR_NVS_NOT_FOUND) // key_not_found is not an error, just return size 0
			ESP_LOGE("NVS_Helper", "NVSHelper::getBlobSize(): err = [0x%X]\n",
					(unsigned int)err);

		close();

		return 0;
	}

	close();

	return required_size;
}

bool NVSHelper::getBlob(string key, uint8_t *blob, size_t length) {
	if (length == 0)
		return false;

	size_t required_size = getBlobSize(key);

	if (required_size == 0)
		return false;

	if (length < required_size)
		return false;

	if (!open()) return false;

	esp_err_t err = nvs_get_blob(_nvs_handle, key.c_str(), blob,
			&required_size);

	if (err) {
		ESP_LOGE("NVS_Helper",
				"NVSHelper::getBlob(): get object err = [0x%X]\n", (unsigned int)err);

		close();
		return false;
	}

	close();
	return true;
}

bool NVSHelper::getBlob(string key, std::vector<uint8_t> &blob) {
	size_t required_size = getBlobSize(key);
	if (required_size == 0)
		return false;

	blob.resize(required_size);

	if (!open()) return false;

	esp_err_t err = nvs_get_blob(_nvs_handle, key.c_str(), &blob[0],
			&required_size);

	if (err) {
		ESP_LOGE("NVS_Helper",
				"NVSHelper::getBlob(): get object err = [0x%X]\n", (unsigned int)err);

		close();

		return false;
	}

	close();

	return true;
}

std::vector<uint8_t> NVSHelper::getBlob(string key) {
	std::vector<uint8_t> res;
	bool ok = getBlob(key, res);
	if (!ok)
		res.clear();
	return res;
}

bool NVSHelper::setFloat(string key, float value, bool forceCommit) {
	return setBlob(key, (uint8_t*) &value, sizeof(float), forceCommit);
}

float NVSHelper::getFloat(string key, float default_value) {
	std::vector<uint8_t> res(sizeof(float));
	if (!getBlob(key, res))
		return default_value;
	return *(float*) (&res[0]);
}

NVSHelper NVS;
