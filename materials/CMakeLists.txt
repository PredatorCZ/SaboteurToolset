project(MaterialsExtract)

build_target(
  NAME
  materials_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  materials_extract.cpp
  LINKS
  precore
  common_obj
  INCLUDES
  ${TPD_PATH}/precore/3rd_party/json
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract materials package"
  START_YEAR
  2023)
