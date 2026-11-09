cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake"
        CACHE PATH "VitaSDK toolchain file")
endif()

project(DShop C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "DShop")
set(VITA_TITLEID "DSHP00001")
set(VITA_VERSION "01.00")

add_executable(${PROJECT_NAME}
    src/main.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -O2
    -Wall
    -Wextra
)

target_link_libraries(${PROJECT_NAME}
    vita2d
    SceCtrl_stub
    SceDisplay_stub
    SceSysmodule_stub
)

vita_create_self(eboot.bin ${PROJECT_NAME})

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME "${VITA_APP_NAME}"
)
