add_library(detours OBJECT
creatwth.cpp
detours.cpp
disasm.cpp
disolarm.cpp
disolx86.cpp
image.cpp
modules.cpp
#uimports.cpp
)
target_include_directories(detours PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
