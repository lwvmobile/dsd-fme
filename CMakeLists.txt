project(dsd-fme)
cmake_minimum_required(VERSION 2.8.11)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
#Set curses to ncurses, and wide true for ascii
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)

include(git_revision)
git_describe(GIT_TAG)

find_package(LibSndFile REQUIRED)
find_package(MBE REQUIRED)
find_package(ITPP REQUIRED)
find_package(RTLSDR)
find_package(Curses REQUIRED) 
find_package(PULSEAUDIO REQUIRED)

include_directories(SYSTEM ${LIBSNDFILE_INCLUDE_DIR} ${MBE_INCLUDE_DIR} ${ITPP_INCLUDE_DIR} ${PULSEAUDIO_INCLUDE_DIRS} ${CURSES_INCLUDE_DIR})
set(LIBS ${MBE_LIBRARY} ${LIBSNDFILE_LIBRARY} ${ITPP_LIBRARY} ${PULSEAUDIO_SIMPLE_LIBRARY} ${CURSES_LIBRARY})

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

ADD_EXECUTABLE(dsd-fme ${SRCS} ${HEADERS})
TARGET_LINK_LIBRARIES(dsd-fme ${LIBS})

include(GNUInstallDirs)
install(TARGETS dsd-fme DESTINATION ${CMAKE_INSTALL_BINDIR})

# man page
find_program(HELP2MAN_FOUND help2man)
if (HELP2MAN_FOUND)
    add_custom_command(TARGET dsd-fme POST_BUILD
        COMMAND help2man
        ARGS -n "Digital Speech Decoder"
            --version-string=${GIT_TAG}
            -o ${CMAKE_CURRENT_BINARY_DIR}/dsd-fme.1
            --no-info
            $<TARGET_FILE:dsd-fme>
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dsd-fme.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

# uninstall target
configure_file(
    "cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
