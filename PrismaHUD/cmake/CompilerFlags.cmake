if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /d2ReducedOptimizeHugeFunctions")
    
    string(APPEND CMAKE_CXX_FLAGS " /Gy")
    string(APPEND CMAKE_CXX_FLAGS " /Gw")
    
    string(APPEND CMAKE_CXX_FLAGS " /Zc:inline")
    
    foreach(flag_var
        CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
        string(REGEX REPLACE "/EH[scra-z]+" "" ${flag_var} "${${flag_var}}")
        string(APPEND ${flag_var} " /EHa /bigobj")
        string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach()
    
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    

endif()