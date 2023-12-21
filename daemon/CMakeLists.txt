add_executable(daemon daemon.c service.c supervisor.c utils.c existing_process_handler.c signal_handler.c constants.h)
set_target_properties(daemon PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
