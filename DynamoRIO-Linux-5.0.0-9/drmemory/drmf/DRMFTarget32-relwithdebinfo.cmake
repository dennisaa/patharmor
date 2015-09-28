#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "drsyscall" for configuration "RelWithDebInfo"
set_property(TARGET drsyscall APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsyscall PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr;drcontainers"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall.so.1.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrsyscall.so.1.0"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyscall )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyscall "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall.so.1.0" )

# Import target "drsyscall_static" for configuration "RelWithDebInfo"
set_property(TARGET drsyscall_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsyscall_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "ASM;C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr_static;drcontainers"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyscall_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyscall_static "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsyscall_static.a" )

# Import target "drsymcache" for configuration "RelWithDebInfo"
set_property(TARGET drsymcache APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsymcache PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELWITHDEBINFO "drsyms"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache.so.1.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrsymcache.so.1.0"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsymcache )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsymcache "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache.so.1.0" )

# Import target "drsymcache_static" for configuration "RelWithDebInfo"
set_property(TARGET drsymcache_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsymcache_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drcontainers;drmgr_static;drsyms_static"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsymcache_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsymcache_static "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libdrsymcache_static.a" )

# Import target "umbra" for configuration "RelWithDebInfo"
set_property(TARGET umbra APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(umbra PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr;drcontainers"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra.so.1.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libumbra.so.1.0"
  )

list(APPEND _IMPORT_CHECK_TARGETS umbra )
list(APPEND _IMPORT_CHECK_FILES_FOR_umbra "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra.so.1.0" )

# Import target "umbra_static" for configuration "RelWithDebInfo"
set_property(TARGET umbra_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(umbra_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr_static;drcontainers"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS umbra_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_umbra_static "${_IMPORT_PREFIX}/drmemory/drmf/lib32/release/libumbra_static.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
