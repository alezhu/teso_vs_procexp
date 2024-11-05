set(HEADER_FILES
        include/consts.h
        include/Logger.h
        include/utils.h
)

set(SOURCE_FILES
        src/Logger.cpp
        src/utils.cpp
)

#add_library(common STATIC ${SOURCE_FILES})
add_library(common ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(common  PUBLIC "-static")
target_include_directories(common  INTERFACE include)
