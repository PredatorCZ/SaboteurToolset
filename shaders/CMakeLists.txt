project(ShadersExtract)

build_target(
  NAME
  shaders_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  shaders_extract.cpp
  LINKS
  spike
  common_obj
  INCLUDES
  ${TPD_PATH}/spike/3rd_party/json
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract shaders package"
  START_YEAR
  2023)
