# - Try to find NCURSES
# Once done this will define
#
#  ITPP_FOUND - System has ITPP
#  ITPP_INCLUDE_DIR - The ITPP include directory
#  ITPP_LIBRARY - The library needed to use ITPP
#

find_path(ITPP_INCLUDE_DIR itpp/itcomm.h)

set(NCURSES_NAMES ${NCURSES_NAMES} curses.h ncurses ncurses5w ncurses6w ncurses6 ncurses5 libncursesw)
FIND_LIBRARY(NCURSES_LIBRARY NAMES ${NCURSES_NAMES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NCURSES DEFAULT_MSG NCURSES_LIBRARY NCURSES_INCLUDE_DIR)
