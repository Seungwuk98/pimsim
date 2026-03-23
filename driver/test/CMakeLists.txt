add_executable(
  TEST_Gemv 
  gemv.cpp
)

target_link_libraries(
  TEST_Gemv
  PRIVATE
  ${llvm_libs}
  PIMSimMemory
  pimsim_driver
  doctest
)

target_include_directories(TEST_Gemv PRIVATE ../src)
