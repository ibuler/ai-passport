cmake_minimum_required(VERSION 3.16)
project(pocket_render C CXX)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../sdkconfig.defaults" pool_setting
     REGEX "^CONFIG_LV_MEM_SIZE_KILOBYTES=[0-9]+$")
if(NOT pool_setting)
    message(FATAL_ERROR "The application LVGL pool size is missing")
endif()
string(REPLACE "CONFIG_LV_MEM_SIZE_KILOBYTES=" "" pool_kb "${pool_setting}")
add_compile_definitions(PF_UI_POOL_KB=${pool_kb})
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "")
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "")
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "")
add_subdirectory("../../managed_components/lvgl__lvgl" lvgl)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(portrait_bytes "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/images/furina/furina.rgb565a8")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/furina_data.c"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/pocket/embed_portrait.py"
        "${portrait_bytes}" "${CMAKE_CURRENT_BINARY_DIR}/furina_data.c"
    DEPENDS "${portrait_bytes}" "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/pocket/embed_portrait.py")
add_executable(pocket-render render.c ../../main/pocket/ui.c ../../main/pocket/model.c
    ../../main/pocket/xiaozhi_model.c ../../main/pocket/furina.c "${CMAKE_CURRENT_BINARY_DIR}/furina_data.c"
    ../../main/pocket/github.c ../../assets/fonts/pocket_font_16.c)
target_compile_definitions(pocket-render PRIVATE PF_HOST_RENDER=1)
target_include_directories(pocket-render PRIVATE ../../main)
target_link_libraries(pocket-render PRIVATE lvgl m)
