include_directories(include)
include_directories(../groupsock/include)
include_directories(../UsageEnvironment/include)

file(GLOB BasicUsageEnvironmentFiles
   *.cpp)
list(APPEND BUE_Dependence_SRC_LIST
            ../UsageEnvironment/HashTable.cpp
            ../UsageEnvironment/strDup.cpp
            ../UsageEnvironment/UsageEnvironment.cpp
        )
add_library(BasicUsageEnvironment SHARED
         ${BasicUsageEnvironmentFiles} ${BUE_Dependence_SRC_LIST})
