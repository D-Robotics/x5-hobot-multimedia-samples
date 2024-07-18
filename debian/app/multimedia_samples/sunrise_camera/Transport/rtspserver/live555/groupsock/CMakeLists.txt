include_directories(include)
include_directories(../UsageEnvironment/include)
include_directories(../BasicUsageEnvironment/include)

file(GLOB groupsockfiles
   *.cpp *.c)
list(APPEND GS_Dependence_SRC_LIST
            ../BasicUsageEnvironment/BasicHashTable.cpp
            ../UsageEnvironment/strDup.cpp
            ../UsageEnvironment/HashTable.cpp
        )
add_library(groupsock SHARED ${groupsockfiles} ${GS_Dependence_SRC_LIST})
