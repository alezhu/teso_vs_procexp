add_executable(teso_wait_service
        src/main.cpp
        src/service.cpp
        src/watcher.cpp
        src/global.cpp
)

target_link_libraries(teso_wait_service PRIVATE common)