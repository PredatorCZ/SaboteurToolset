project(TilePack)

build_target(
  NAME
  tilepack_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  tilepack_extract.cpp
  LINKS
  precore
  zlib_obj
  common_obj
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract MegaPacks"
  START_YEAR
  2023)
