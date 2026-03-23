add_library(
  PIMSimMemory 
  STATIC
  Memory.cpp
  REPL.cpp
  DefaultDRAM.cpp
  Newton.cpp
  TypeSupport.cpp
  Controller.cpp
  Neupims.cpp
  Context.cpp
)

target_link_libraries(PIMSimMemory PRIVATE dramsim3 ${llvm_libs})
