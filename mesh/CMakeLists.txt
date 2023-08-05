project(MESH2GLTF)

build_target(
  NAME
  mesh_to_gltf
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  mesh_to_gltf.cpp
  LINKS
  spike
  common_obj
  gltf
  AUTHOR
  "Lukas Cone"
  DESCR
  "Convert mesh to gltf"
  START_YEAR
  2023)
