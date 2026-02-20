add_executable(
  pimsim
  simulator.cpp
)

target_link_libraries(
  pimsim
  PRIVATE
  ${llvm_libs}
  PIMSimMemory
)
