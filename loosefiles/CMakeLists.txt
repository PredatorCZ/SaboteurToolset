project(LooseFiles)

build_target(
  NAME
  loosefiles_extract
  TYPE
  ESMODULE
  VERSION
  1
  SOURCES
  loosefiles_extract.cpp
  LINKS
  precore
  AUTHOR
  "Lukas Cone"
  DESCR
  "Extract LooseFiles"
  START_YEAR
  2023)
