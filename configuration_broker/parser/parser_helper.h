// Copyright Configured Things and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#pragma once

#include <compartment.h>
#include <magic_enum/magic_enum.hpp>
#include <stddef.h>
#include <string.h>

#include "./core_json.h"

// /////////////////////////////////////////////
//
// Helper functions for extracting types from JSON
//
// /////////////////////////////////////////////

//
// Helper function to extract a string value
//
bool get_string(const char *json, const char *key, char *dst)
{
	char  *value;
	size_t valueLength;

	auto result = JSON_Search((char *)json,
	                          strlen(json),
	                          (char *)key,
	                          strlen(key),
	                          &value,
	                          &valueLength);

	if (result != JSONSuccess)
	{
		Debug::log("Missing key {} in {}", key, json);
		return false;
	}

	strncpy(dst, value, valueLength);
	dst[valueLength] = '\0';
	return true;
}

//
// Helper function to extract a number value
//
template<class T>
bool get_number(const char *json, const char *key, T *dst)
{
	char  *value;
	size_t valueLength;

	auto result = JSON_Search((char *)json,
	                          strlen(json),
	                          (char *)key,
	                          strlen(key),
	                          &value,
	                          &valueLength);

	if (result != JSONSuccess)
	{
		Debug::log("Missing key {} in {}", key, json);
		return false;
	}

	bool  isNumber = true;
	int   acc      = 0;
	char *str      = value;
	for (int i = 0; i < valueLength && str; i++, str++)
	{
		if (isNumber && isdigit(*str))
		{
			isNumber = true;
			acc *= 10;
			acc += *str - 0x30;
		}
		else
		{
			isNumber = false;
		}
	}

	if (!isNumber)
	{
		Debug::log("{} is not a number", key);
		return false;
	}

	*dst = acc;
	// Check we haven't lost something in the cast
	if (*dst != acc)
	{
		Debug::log("Value truncated {} -> {}", acc, *dst);
		return false;
	}

	return true;
}

//
// Helper function to extract an enum from a string
//
template<class T>
bool get_enum(const char *json, const char *key, T *dst)
{
	char  *value;
	size_t valueLength;

	auto result = JSON_Search((char *)json,
	                          strlen(json),
	                          (char *)key,
	                          strlen(key),
	                          &value,
	                          &valueLength);

	if (result != JSONSuccess)
	{
		Debug::log("Missing key {} in {}", key, json);
		return false;
	}

	std::string enum_string = std::string(value, valueLength);
	auto        m =
	  magic_enum::enum_cast<T>(enum_string, magic_enum::case_insensitive);
	if (!m.has_value())
	{
		Debug::log("Invalid emum value {} for {}", enum_string, key);
		return false;
	}

	*dst = m.value();
	return true;
}