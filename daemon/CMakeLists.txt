add_executable(daemon daemon.c service.c supervisor.c utils.c)
set_target_properties(daemon PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
