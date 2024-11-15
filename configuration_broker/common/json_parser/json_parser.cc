// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// A simple wrapper for coreJSON from FreeRTOS to make
// it into a CHERIoT library

#include "../third_party/coreJSON/core_json.h"

namespace jsonParser
{

	/**
	 * Validate that a string is serialised JSON
	 */
	JSONStatus_t __cheri_libcall validate(const char *buf, size_t max)
	{
		return JSON_Validate(buf, max);
	}

	/*
	 * Set outValue to point to the start of a key (query) in
	 * a JSON string (buf)
	 */
	JSONStatus_t __cheri_libcall search(char       *buf,
	                                    size_t      max,
	                                    const char *query,
	                                    size_t      queryLength,
	                                    char      **outValue,
	                                    size_t     *outValueLength)
	{
		return JSON_Search(
		  buf, max, query, queryLength, outValue, outValueLength);
	}

} // namespace jsonParser
