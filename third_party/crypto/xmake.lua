compartment("crypto")
    add_deps("freestanding", "debug", "cxxrt")

    add_files("crypto.cc")
    add_files("rand_32.cc")
    add_files("./libhydrogen/hydrogen.c")    
