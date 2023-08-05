project(MegaPack)

build_target(
  NAME
  megapack_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  megapack_extract.cpp
  LINKS
  spike
  common_obj
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract MegaPacks"
  START_YEAR
  2023)
