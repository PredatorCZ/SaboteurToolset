project(AnimPack)

build_target(
  NAME
  anim_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  anim_extract.cpp
  LINKS
  spike
  common_obj
  zlib_obj
  INCLUDES
  ${TPD_PATH}/spike/3rd_party/json
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract animations.pack"
  START_YEAR
  2023)
