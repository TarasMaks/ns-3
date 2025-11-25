build_lib(
  LIBNAME ble
  SOURCE_FILES
    model/ble-propagation-loss-model.cc
    model/ble-phy.cc
  HEADER_FILES
    model/ble-propagation-loss-model.h
    model/ble-phy.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libmobility}
    ${libspectrum}
    ${libnetwork}
    ${libantenna}
  TEST_SOURCES
)
