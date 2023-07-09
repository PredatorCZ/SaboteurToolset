project(GlobalExtract)

build_target(
  NAME
  global_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  global_extract.cpp
  LINKS
  precore
  zlib_obj
  common_obj
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract global.map"
  START_YEAR
  2023)
