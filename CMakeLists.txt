cmake_minimum_required(VERSION 3.1.0)
project(CnDfu)
option(USB_DIR "USB_DIR" "empty")
option(FIBER_DIR "FIB_DIR" "empty")
# getopt 直接源码集成
include_directories("${USB_DIR}/include/libusb-1.0" "${FIBER_DIR}/include")
link_directories("${USB_DIR}/VS2017/MS64/dll" "${FIBER_DIR}")
# 预置宏
# -D_OFF_T_DEFINED 
add_definitions( -DHAVE_WINDOWS_H 
-D_CRT_SECURE_NO_WARNINGS
-D_CRT_NONSTDC_NO_DEPRECATE
-DHAVE_STRING_H
-D_WINSOCK_DEPRECATED_NO_WARNINGS)

FILE(GLOB C_SRC src/*.c)
FILE(GLOB HEADER src/*.h)
FILE(GLOB CC_SRC src/*.cc src/*.cpp src/*.hpp)

source_group(sources FILES ${C_SRC} ${CC_SRC})
source_group(headers FILES ${HEADER})

SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/)
add_executable(${PROJECT_NAME} ${C_SRC} ${HEADER} ${CC_SRC})
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/Zi>")
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/EHa>")
target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
target_link_libraries(${PROJECT_NAME} libusb-1.0.lib libfiber.lib ws2_32.lib)