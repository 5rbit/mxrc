# FindEtherCAT.cmake
# Finds IgH EtherCAT Master library
#
# Usage:
#   find_package(EtherCAT)
#
# Defines:
#   EtherCAT_FOUND - System has IgH EtherCAT Master
#   EtherCAT_INCLUDE_DIRS - Include directories
#   EtherCAT_LIBRARIES - Libraries to link

find_path(EtherCAT_INCLUDE_DIR
    NAMES ecrt.h
    PATHS
        /usr/local/include
        /usr/include
        /opt/etherlab/include
    PATH_SUFFIXES ethercat
)

find_library(EtherCAT_LIBRARY
    NAMES ethercat
    PATHS
        /usr/local/lib
        /usr/lib
        /opt/etherlab/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(EtherCAT
    REQUIRED_VARS EtherCAT_LIBRARY EtherCAT_INCLUDE_DIR
)

if(EtherCAT_FOUND)
    set(EtherCAT_LIBRARIES ${EtherCAT_LIBRARY})
    set(EtherCAT_INCLUDE_DIRS ${EtherCAT_INCLUDE_DIR})

    if(NOT TARGET EtherCAT::EtherCAT)
        add_library(EtherCAT::EtherCAT UNKNOWN IMPORTED)
        set_target_properties(EtherCAT::EtherCAT PROPERTIES
            IMPORTED_LOCATION "${EtherCAT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${EtherCAT_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(EtherCAT_INCLUDE_DIR EtherCAT_LIBRARY)
