
include(generate_product_version)
generate_product_version(ProductVersionFiles
        NAME ${OUTPUT_NAME}
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/product.ico"
        VERSION_MAJOR 1
        VERSION_MINOR 0
        VERSION_PATCH 0
        VERSION_REVISION 0
        COMPANY_NAME alezhu)
add_executable(teso_wait_service
        src/main.cpp
        src/service.cpp
        src/watcher.cpp
        src/global.cpp
        ${ProductVersionFiles}
)

target_link_libraries(teso_wait_service PRIVATE common)