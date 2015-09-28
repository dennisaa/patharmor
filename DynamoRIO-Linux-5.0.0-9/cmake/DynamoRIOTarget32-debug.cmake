#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dynamorio" for configuration "Debug"
set_property(TARGET dynamorio APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(dynamorio PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG ""
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib32/debug/libdynamorio.so.5.0"
  IMPORTED_SONAME_DEBUG "libdynamorio.so.5.0"
  )

list(APPEND _IMPORT_CHECK_TARGETS dynamorio )
list(APPEND _IMPORT_CHECK_FILES_FOR_dynamorio "${_IMPORT_PREFIX}/lib32/debug/libdynamorio.so.5.0" )

# Import target "drinjectlib" for configuration "Debug"
set_property(TARGET drinjectlib APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drinjectlib PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "drdecode"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib32/libdrinjectlib.so"
  IMPORTED_SONAME_DEBUG "libdrinjectlib.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drinjectlib )
list(APPEND _IMPORT_CHECK_FILES_FOR_drinjectlib "${_IMPORT_PREFIX}/lib32/libdrinjectlib.so" )

# Import target "drdecode" for configuration "Debug"
set_property(TARGET drdecode APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drdecode PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "ASM;C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib32/debug/libdrdecode.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drdecode )
list(APPEND _IMPORT_CHECK_FILES_FOR_drdecode "${_IMPORT_PREFIX}/lib32/debug/libdrdecode.a" )

# Import target "drconfiglib" for configuration "Debug"
set_property(TARGET drconfiglib APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drconfiglib PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib32/libdrconfiglib.so"
  IMPORTED_SONAME_DEBUG "libdrconfiglib.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drconfiglib )
list(APPEND _IMPORT_CHECK_FILES_FOR_drconfiglib "${_IMPORT_PREFIX}/lib32/libdrconfiglib.so" )

# Import target "drfrontendlib" for configuration "Debug"
set_property(TARGET drfrontendlib APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drfrontendlib PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "drdecode"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin32/libdrfrontendlib.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drfrontendlib )
list(APPEND _IMPORT_CHECK_FILES_FOR_drfrontendlib "${_IMPORT_PREFIX}/bin32/libdrfrontendlib.a" )

# Import target "drcontainers" for configuration "Debug"
set_property(TARGET drcontainers APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drcontainers PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrcontainers.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drcontainers )
list(APPEND _IMPORT_CHECK_FILES_FOR_drcontainers "${_IMPORT_PREFIX}/ext/lib32/debug/libdrcontainers.a" )

# Import target "drutil" for configuration "Debug"
set_property(TARGET drutil APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drutil PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio;drmgr"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrutil.so"
  IMPORTED_SONAME_DEBUG "libdrutil.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drutil )
list(APPEND _IMPORT_CHECK_FILES_FOR_drutil "${_IMPORT_PREFIX}/ext/lib32/debug/libdrutil.so" )

# Import target "drutil_static" for configuration "Debug"
set_property(TARGET drutil_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drutil_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio;drmgr_static"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrutil_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drutil_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drutil_static "${_IMPORT_PREFIX}/ext/lib32/debug/libdrutil_static.a" )

# Import target "drgui" for configuration "Debug"
set_property(TARGET drgui APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drgui PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/bin32/drgui"
  )

list(APPEND _IMPORT_CHECK_TARGETS drgui )
list(APPEND _IMPORT_CHECK_FILES_FOR_drgui "${_IMPORT_PREFIX}/ext/bin32/drgui" )

# Import target "drsyms" for configuration "Debug"
set_property(TARGET drsyms APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drsyms PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "dynamorio"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG ""
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrsyms.so"
  IMPORTED_SONAME_DEBUG "libdrsyms.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyms )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyms "${_IMPORT_PREFIX}/ext/lib32/debug/libdrsyms.so" )

# Import target "drsyms_static" for configuration "Debug"
set_property(TARGET drsyms_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drsyms_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C;CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio;drcontainers;dwarf;elftc;elf"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrsyms_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyms_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyms_static "${_IMPORT_PREFIX}/ext/lib32/debug/libdrsyms_static.a" )

# Import target "drwrap" for configuration "Debug"
set_property(TARGET drwrap APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drwrap PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio;drmgr;drcontainers"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrwrap.so"
  IMPORTED_SONAME_DEBUG "libdrwrap.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drwrap )
list(APPEND _IMPORT_CHECK_FILES_FOR_drwrap "${_IMPORT_PREFIX}/ext/lib32/debug/libdrwrap.so" )

# Import target "drwrap_static" for configuration "Debug"
set_property(TARGET drwrap_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drwrap_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "ASM;C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio;drmgr_static;drcontainers"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrwrap_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drwrap_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drwrap_static "${_IMPORT_PREFIX}/ext/lib32/debug/libdrwrap_static.a" )

# Import target "drx" for configuration "Debug"
set_property(TARGET drx APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drx PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio;drcontainers;drmgr"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrx.so"
  IMPORTED_SONAME_DEBUG "libdrx.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drx )
list(APPEND _IMPORT_CHECK_FILES_FOR_drx "${_IMPORT_PREFIX}/ext/lib32/debug/libdrx.so" )

# Import target "drx_static" for configuration "Debug"
set_property(TARGET drx_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drx_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio;drcontainers;drmgr_static"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrx_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drx_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drx_static "${_IMPORT_PREFIX}/ext/lib32/debug/libdrx_static.a" )

# Import target "drmgr" for configuration "Debug"
set_property(TARGET drmgr APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drmgr PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrmgr.so"
  IMPORTED_SONAME_DEBUG "libdrmgr.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drmgr )
list(APPEND _IMPORT_CHECK_FILES_FOR_drmgr "${_IMPORT_PREFIX}/ext/lib32/debug/libdrmgr.so" )

# Import target "drmgr_static" for configuration "Debug"
set_property(TARGET drmgr_static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(drmgr_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_DEBUG "dynamorio"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/ext/lib32/debug/libdrmgr_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drmgr_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drmgr_static "${_IMPORT_PREFIX}/ext/lib32/debug/libdrmgr_static.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
