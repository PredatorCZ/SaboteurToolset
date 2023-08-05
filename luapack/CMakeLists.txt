project(LuaPack)

build_target(
  NAME
  luap_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  luap_extract.cpp
  LINKS
  spike
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract luap packages"
  START_YEAR
  2023)
