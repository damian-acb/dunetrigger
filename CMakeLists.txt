# ======================================================================
#  dunetrigger main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/dunetrigger/ups/setup_for_development <-d|-p>
#  cmake [-DCMAKE_INSTALL_PREFIX=/install/path]
#        -DCMAKE_BUILD_TYPE=$CETPKG_TYPE
#        $CETPKG_SOURCE
#  make
#  make test
#  make install
#  make package (builds distribution tarfile)
# ======================================================================


# use cmake 3.12 or later
cmake_minimum_required (VERSION 3.20 FATAL_ERROR)

project(dunetrigger LANGUAGES CXX C)
set(${PROJECT_NAME}_CMAKE_PROJECT_VERSION_STRING 10.24.00d00)

# cetbuildtools contains our cmake modules
find_package(cetmodules REQUIRED)

set(${PROJECT_NAME}_FW_DIR "config_data")

# Project variable defaults (_INIT) and overrides.
set(${PROJECT_NAME}_SCRIPTS_DIR_INIT scripts)

include(CetCMakeEnv)
cet_cmake_env()

# Find our own CMake modules, and tell everyone else where to find them.
cet_cmake_module_directories(Modules BINARY)

cet_set_compiler_flags(DIAGS CAUTIOUS
  WERROR
  NO_UNDEFINED
  EXTRA_FLAGS -pedantic -Wno-unused-local-typedefs
)
cet_report_compiler_flags()

find_package( nufinder REQUIRED )
find_package( larcore REQUIRED EXPORT)
find_package( lardata REQUIRED EXPORT )
find_package( lardataalg REQUIRED EXPORT )
find_package( nuevdb REQUIRED EXPORT )
find_package( nug4 REQUIRED EXPORT )
find_package( nugen REQUIRED EXPORT )
find_package( nurandom REQUIRED EXPORT )
find_package( nusimdata REQUIRED EXPORT )
find_package( nutools REQUIRED EXPORT )
find_package( art 3.09.00 REQUIRED )
find_package( art_root_io REQUIRED )
find_package( ROOT REQUIRED )
find_package( dunedaqdataformats REQUIRED EXPORT )
find_package( HDF5 REQUIRED)
find_package( hep_hpc REQUIRED EXPORT )
find_ups_product( nlohmann_json ) # this seems to be deprecated but I can't find other working ways
find_ups_product( TRACE ) # this seems to be deprecated but I can't find other working ways

#dune packages
find_package( dunecore REQUIRED EXPORT )
find_package( dunedetdataformats REQUIRED EXPORT )

# macros for dictionary and simple_plugin
include(ArtDictionary)
include(ArtMake)
include(BuildPlugins)

# add cet_find_library commands here when needed

# ADD SOURCE CODE SUBDIRECTORIES HERE
add_subdirectory(dunetrigger)

# tests
add_subdirectory(test)

# ups - table and config files
# must be AFTER all other subdirectories
add_subdirectory(ups)

# packaging utility - must be last
include(UseCPack)
