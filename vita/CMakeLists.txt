cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK")
  endif()
endif()

project(discord_vita)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Discord Vita")
set(VITA_TITLEID "DISCVITA1")
set(VITA_VERSION "01.70")

add_executable(${PROJECT_NAME}
  src/main.c
  src/ui.c
  src/net.c
  src/api.c
  src/session.c
  src/input.c
  src/ime.c
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME}
  vita2d
  SceDisplay_stub
  SceGxm_stub
  SceSysmodule_stub
  SceCtrl_stub
  SceTouch_stub
  ScePgf_stub
  ScePvf_stub
  SceCommonDialog_stub
  SceIme_stub
  SceAppUtil_stub
  SceAppMgr_stub
  SceProcessmgr_stub
  SceNet_stub
  SceNetCtl_stub
  SceHttp_stub
  SceSsl_stub
  SceIofilemgr_stub
  freetype
  png
  jpeg
  z
  m
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
