build_example(
  NAME ble-simple
  SOURCE_FILES ble-simple.cc
  LIBRARIES_TO_LINK
    ${libcore}
    ${libmobility}
    ${libspectrum}
    ${libpropagation}
    ${libnetwork}
    ${libble}
)
