include_directories(include)
include_directories(../groupsock/include)
include_directories(../UsageEnvironment/include)
include_directories(../BasicUsageEnvironment/include)
ADD_DEFINITIONS(-D LOCALE_NOT_USED)
file(GLOB liveMediaFiles
      *.cpp *.c)
list(APPEND LM_Dependence_SRC_LIST
            ../UsageEnvironment/HashTable.cpp
            ../UsageEnvironment/strDup.cpp
            ../groupsock/GroupsockHelper.cpp
            ../groupsock/Groupsock.cpp
            ../groupsock/inet.c
            ../groupsock/NetAddress.cpp
            ../groupsock/NetInterface.cpp
            ../groupsock/GroupEId.cpp
            ../BasicUsageEnvironment/BasicHashTable.cpp
        )
add_library(liveMedia SHARED ${liveMediaFiles} ${LM_Dependence_SRC_LIST})