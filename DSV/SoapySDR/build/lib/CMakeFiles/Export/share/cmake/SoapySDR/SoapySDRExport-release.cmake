#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "SoapySDR" for configuration "Release"
set_property(TARGET SoapySDR APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(SoapySDR PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libSoapySDR.so.0.8.0"
  IMPORTED_SONAME_RELEASE "libSoapySDR.so.0.8"
  )

list(APPEND _IMPORT_CHECK_TARGETS SoapySDR )
list(APPEND _IMPORT_CHECK_FILES_FOR_SoapySDR "${_IMPORT_PREFIX}/lib/libSoapySDR.so.0.8.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
