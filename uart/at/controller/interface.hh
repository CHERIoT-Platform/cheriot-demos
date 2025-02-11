// Copyright 3bian Limited and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <compartment-macros.h>
#include <string>

void __cheri_compartment("display") log(const std::string &line);