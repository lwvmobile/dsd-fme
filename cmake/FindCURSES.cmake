# - Try to find NCURSES
# Once done this will define
#
#  ITPP_FOUND - System has ITPP
#  ITPP_INCLUDE_DIR - The ITPP include directory
#  ITPP_LIBRARY - The library needed to use ITPP
#
set(CURSES_LIBRARY "/opt/lib/libncurses.so")
set(CURSES_INCLUDE_PATH "/opt/include")
find_path(CURSES_INCLUDE_DIR /usr/ncurses.h)

set(CURSES_NAMES ${CURSES_NAMES} curses.h curses curses5w curses6w curses6 curses5 libncursesw)
FIND_LIBRARY(CURSES_LIBRARY NAMES ${CURSES_NAMES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CURSES DEFAULT_MSG NCURSES_LIBRARY CURSES_INCLUDE_DIR)
