add_subdirectory(doctest)
add_subdirectory(DRAMsim3)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/DRAMsim3/configs DESTINATION ${PROJECT_BINARY_DIR})
