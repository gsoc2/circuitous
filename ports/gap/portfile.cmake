vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO lifting-bits/gap
  REF 76127ee540106b1020e840df4e21f9e0bb28993c
  SHA512 c0af50abebcaccb148303a40a3e4115cb3f5eeea6403bb0d33146f0cd19c85df939c42240f5065aaae67482a1f4b6d43250325aff7511f720542cdb9f2060449
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    -DGAP_ENABLE_COROUTINES=ON
    -DGAP_ENABLE_TESTING=OFF
    -DGAP_ENABLE_EXAMPLES=OFF
    -DGAP_INSTALL=ON
    -DUSE_SYSTEM_DEPENDENCIES=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
  PACKAGE_NAME "gap"
  CONFIG_PATH lib/cmake/gap
)

file( REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug" )

# we do not populate lib folder yet
file( REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib" )

file(
  INSTALL "${SOURCE_PATH}/LICENSE"
  DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
  RENAME copyright
)

file(
  INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
  DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
)