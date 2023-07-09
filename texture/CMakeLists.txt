project(DTEX2DDS)

build_target(
  NAME
  dtex_to_dds
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  dtex_to_dds.cpp
  LINKS
  precore
  zlib_obj
  AUTHOR
  "Lukas Cone"
  DESCR
  "Convert dtex to dds"
  START_YEAR
  2023)
