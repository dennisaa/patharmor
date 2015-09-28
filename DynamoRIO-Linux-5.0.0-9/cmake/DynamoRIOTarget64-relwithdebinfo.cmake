#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dynamorio" for configuration "RelWithDebInfo"
set_property(TARGET dynamorio APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(dynamorio PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO ""
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib64/release/libdynamorio.so.5.0"
  IMPORTED_SONAME_RELWITHDEBINFO "libdynamorio.so.5.0"
  )

list(APPEND _IMPORT_CHECK_TARGETS dynamorio )
list(APPEND _IMPORT_CHECK_FILES_FOR_dynamorio "${_IMPORT_PREFIX}/lib64/release/libdynamorio.so.5.0" )

# Import target "drinjectlib" for configuration "RelWithDebInfo"
set_property(TARGET drinjectlib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drinjectlib PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "drdecode"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib64/libdrinjectlib.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrinjectlib.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drinjectlib )
list(APPEND _IMPORT_CHECK_FILES_FOR_drinjectlib "${_IMPORT_PREFIX}/lib64/libdrinjectlib.so" )

# Import target "drdecode" for configuration "RelWithDebInfo"
set_property(TARGET drdecode APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drdecode PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "ASM;C"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib64/release/libdrdecode.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drdecode )
list(APPEND _IMPORT_CHECK_FILES_FOR_drdecode "${_IMPORT_PREFIX}/lib64/release/libdrdecode.a" )

# Import target "drconfiglib" for configuration "RelWithDebInfo"
set_property(TARGET drconfiglib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drconfiglib PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib64/libdrconfiglib.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrconfiglib.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drconfiglib )
list(APPEND _IMPORT_CHECK_FILES_FOR_drconfiglib "${_IMPORT_PREFIX}/lib64/libdrconfiglib.so" )

# Import target "drfrontendlib" for configuration "RelWithDebInfo"
set_property(TARGET drfrontendlib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drfrontendlib PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "drdecode"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/bin64/libdrfrontendlib.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drfrontendlib )
list(APPEND _IMPORT_CHECK_FILES_FOR_drfrontendlib "${_IMPORT_PREFIX}/bin64/libdrfrontendlib.a" )

# Import target "drcontainers" for configuration "RelWithDebInfo"
set_property(TARGET drcontainers APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drcontainers PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrcontainers.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drcontainers )
list(APPEND _IMPORT_CHECK_FILES_FOR_drcontainers "${_IMPORT_PREFIX}/ext/lib64/release/libdrcontainers.a" )

# Import target "drutil" for configuration "RelWithDebInfo"
set_property(TARGET drutil APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drutil PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrutil.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrutil.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drutil )
list(APPEND _IMPORT_CHECK_FILES_FOR_drutil "${_IMPORT_PREFIX}/ext/lib64/release/libdrutil.so" )

# Import target "drutil_static" for configuration "RelWithDebInfo"
set_property(TARGET drutil_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drutil_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr_static"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrutil_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drutil_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drutil_static "${_IMPORT_PREFIX}/ext/lib64/release/libdrutil_static.a" )

# Import target "drgui" for configuration "RelWithDebInfo"
set_property(TARGET drgui APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drgui PROPERTIES
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/bin64/drgui"
  )

list(APPEND _IMPORT_CHECK_TARGETS drgui )
list(APPEND _IMPORT_CHECK_FILES_FOR_drgui "${_IMPORT_PREFIX}/ext/bin64/drgui" )

# Import target "drsyms" for configuration "RelWithDebInfo"
set_property(TARGET drsyms APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsyms PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELWITHDEBINFO "dynamorio"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO ""
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrsyms.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrsyms.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyms )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyms "${_IMPORT_PREFIX}/ext/lib64/release/libdrsyms.so" )

# Import target "drsyms_static" for configuration "RelWithDebInfo"
set_property(TARGET drsyms_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drsyms_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C;CXX"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drcontainers;dwarf;elftc;elf"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrsyms_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drsyms_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drsyms_static "${_IMPORT_PREFIX}/ext/lib64/release/libdrsyms_static.a" )

# Import target "drwrap" for configuration "RelWithDebInfo"
set_property(TARGET drwrap APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drwrap PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr;drcontainers"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrwrap.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrwrap.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drwrap )
list(APPEND _IMPORT_CHECK_FILES_FOR_drwrap "${_IMPORT_PREFIX}/ext/lib64/release/libdrwrap.so" )

# Import target "drwrap_static" for configuration "RelWithDebInfo"
set_property(TARGET drwrap_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drwrap_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "ASM;C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drmgr_static;drcontainers"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrwrap_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drwrap_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drwrap_static "${_IMPORT_PREFIX}/ext/lib64/release/libdrwrap_static.a" )

# Import target "drx" for configuration "RelWithDebInfo"
set_property(TARGET drx APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drx PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drcontainers;drmgr"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrx.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrx.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drx )
list(APPEND _IMPORT_CHECK_FILES_FOR_drx "${_IMPORT_PREFIX}/ext/lib64/release/libdrx.so" )

# Import target "drx_static" for configuration "RelWithDebInfo"
set_property(TARGET drx_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drx_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio;drcontainers;drmgr_static"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrx_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drx_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drx_static "${_IMPORT_PREFIX}/ext/lib64/release/libdrx_static.a" )

# Import target "drmgr" for configuration "RelWithDebInfo"
set_property(TARGET drmgr APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drmgr PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrmgr.so"
  IMPORTED_SONAME_RELWITHDEBINFO "libdrmgr.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS drmgr )
list(APPEND _IMPORT_CHECK_FILES_FOR_drmgr "${_IMPORT_PREFIX}/ext/lib64/release/libdrmgr.so" )

# Import target "drmgr_static" for configuration "RelWithDebInfo"
set_property(TARGET drmgr_static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(drmgr_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "C"
  IMPORTED_LINK_INTERFACE_LIBRARIES_RELWITHDEBINFO "dynamorio"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/ext/lib64/release/libdrmgr_static.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS drmgr_static )
list(APPEND _IMPORT_CHECK_FILES_FOR_drmgr_static "${_IMPORT_PREFIX}/ext/lib64/release/libdrmgr_static.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
