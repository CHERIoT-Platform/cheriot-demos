-- Copyright Configured Things Ltd and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

-- library for JSON parser   
library("json_parser")
    set_default(false)
    add_files("json_parser.cc")
    add_files("../third_party/coreJSON/core_json.cc")
