include_directories(include)
include_directories(../BasicUsageEnvironment/include)
include_directories(../groupsock/include)

file(GLOB UsageEnvironmentFiles
    *.cpp)
list(APPEND UE_Dependence_SRC_LIST
            ../BasicUsageEnvironment/BasicHashTable.cpp
        )
add_library(UsageEnvironment SHARED ${UsageEnvironmentFiles} ${UE_Dependence_SRC_LIST})
