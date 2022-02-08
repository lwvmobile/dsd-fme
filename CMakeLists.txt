project(dsd)
cmake_minimum_required(VERSION 2.8.11)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

include(git_revision)
git_describe(GIT_TAG)

find_package(LibSndFile REQUIRED)
find_package(MBE REQUIRED)
find_package(ITPP REQUIRED)
#find_package(LibPortAudio) #disabled for testing
find_package(RTLSDR)
#more messing around
#find_package(Curses REQUIRED)
find_package(PULSEAUDIO)

#include_directories(SYSTEM ${LIBSNDFILE_INCLUDE_DIR} ${MBE_INCLUDE_DIR} ${ITPP_INCLUDE_DIR} ${CURSES_INCLUDE_DIR})
#set(LIBS ${MBE_LIBRARY} ${LIBSNDFILE_LIBRARY} ${ITPP_LIBRARY} ${CURSES_LIBRARY})

include_directories(SYSTEM ${LIBSNDFILE_INCLUDE_DIR} ${MBE_INCLUDE_DIR} ${ITPP_INCLUDE_DIR} ${PULSEAUDIO_INCLUDE_DIRS} )
set(LIBS ${MBE_LIBRARY} ${LIBSNDFILE_LIBRARY} ${ITPP_LIBRARY} ${PULSEAUDIO_SIMPLE_LIBRARY})

#going to disable PortAudio, works just fine with padsp in Linux,
#hoping disabling this doesn't break OSX or cygwin, etc builds

#if(PORTAUDIO_FOUND)
#    include_directories(SYSTEM ${PORTAUDIO_INCLUDE_DIRS})
#    list(APPEND LIBS ${PORTAUDIO_LIBRARIES})
#    add_definitions(-DUSE_PORTAUDIO)
#endif(PORTAUDIO_FOUND)

#if(PULSEAUDIO_FOUND) ##I don't think this will be necessary
#    include_directories(SYSTEM ${PULSEAUDIO_INCLUDE_DIRS})
#    list(APPEND LIBS ${PULSEAUDIO_LIBRARIES})
#    add_definitions(-DUSE_PULSEAUDIO)
#endif(PULSEAUDIO_FOUND)

if(RTLSDR_FOUND)
    find_package(Threads)
    include_directories(SYSTEM ${RTLSDR_INCLUDE_DIRS})
    list(APPEND LIBS ${RTLSDR_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    add_definitions(-DUSE_RTLSDR)
endif(RTLSDR_FOUND)

FILE(GLOB SRCS src/*.c src/*.cpp)
FILE(GLOB HEADERS include/*.h include/*.hpp)

if(NOT RTLSDR_FOUND)
    list(REMOVE_ITEM SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/rtl_sdr_fm.cpp)
endif(NOT RTLSDR_FOUND)

configure_file("src/git_ver.c.in" "${CMAKE_CURRENT_BINARY_DIR}/git_ver.c" @ONLY)
list(APPEND SRCS "${CMAKE_CURRENT_BINARY_DIR}/git_ver.c")

include_directories("${PROJECT_SOURCE_DIR}/include")

ADD_EXECUTABLE(dsd ${SRCS} ${HEADERS})
TARGET_LINK_LIBRARIES(dsd ${LIBS})

include(GNUInstallDirs)
install(TARGETS dsd DESTINATION ${CMAKE_INSTALL_BINDIR})

# man page
find_program(HELP2MAN_FOUND help2man)
if (HELP2MAN_FOUND)
    add_custom_command(TARGET dsd POST_BUILD
        COMMAND help2man
        ARGS -n "Digital Speech Decoder"
            --version-string=${GIT_TAG}
            -o ${CMAKE_CURRENT_BINARY_DIR}/dsd.1
            --no-info
            $<TARGET_FILE:dsd>
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dsd.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

# uninstall target
configure_file(
    "cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

option(DISABLE_TEST "Disable building of test framework." OFF)

if (NOT DISABLE_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
