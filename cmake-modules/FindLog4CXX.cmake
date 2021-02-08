
# Include dir
find_path(Log4CXX_INCLUDE_DIR
  NAMES log4cxx/appender.h
  PATHS ${Log4CXX_PKGCONF_INCLUDE_DIRS}
  )

# Finally the library itself
find_library(Log4CXX_LIBRARIES
  NAMES log4cxx
  PATHS ${Log4CXX_PKGCONF_LIBRARY_DIRS}
  )


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Log4CXX
  FOUND_VAR Log4CXX_FOUND
  REQUIRED_VARS Log4CXX_INCLUDE_DIR Log4CXX_LIBRARIES
  )


