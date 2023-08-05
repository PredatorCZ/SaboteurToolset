project(FranceExtract)

build_target(
  NAME
  france_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  france_extract.cpp
  LINKS
  spike
  zlib_obj
  common_obj
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract france.map"
  START_YEAR
  2023)
