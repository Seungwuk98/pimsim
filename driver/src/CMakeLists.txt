
add_library(
  pimsim_driver
  SHARED
  Allocator.cpp
  Driver.cpp
)

target_include_directories(pimsim_driver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
  pimsim_driver 
  PUBLIC 
  ${llvm_libs}
  PIMSimMemory
)

target_compile_options(pimsim_driver PRIVATE -fPIC)

install(FILES API.h DESTINATION include/pimsim/driver)
install(TARGETS pimsim_driver DESTINATION lib)


