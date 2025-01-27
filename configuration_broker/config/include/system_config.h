// Copyright Configured Things Ltd and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <stdlib.h>

// System configurartion data, such as
// id and current switch settings

namespace systemConfig
{

	const auto IdLength = 16;

	struct Config
	{
		char id[IdLength];
		bool switches[8];
	};

} // namespace systemConfig
