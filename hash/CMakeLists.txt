cmake_minimum_required(VERSION 3.3)

project(HashString VERSION 1.0)

build_target(
  NAME
  hash_string
  TYPE
  APP
  SOURCES
  hash_string.cpp
  LINKS
  common_obj
  AUTHOR
  "Lukas Cone"
  DESCR
  "Hash String"
  START_YEAR
  2023)

install(
  TARGETS hash_string
  RUNTIME DESTINATION .)
