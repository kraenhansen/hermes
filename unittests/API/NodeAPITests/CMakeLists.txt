# add_lit_testsuite(test-node-api "Running the Hermes Node API tests"
#   ${CMAKE_CURRENT_SOURCE_DIR}/test
#   ${CMAKE_CURRENT_SOURCE_DIR}/unittests
#   PARAMS ${HERMES_LIT_TEST_PARAMS}
#   DEPENDS ${HERMES_TEST_DEPS}
#   ARGS ${HERMES_TEST_EXTRA_ARGS}
# )

if(HERMES_BUILD_NODE_API_TESTS)
  execute_process(
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/transform.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND_ERROR_IS_FATAL ANY
    COMMAND_ECHO STDOUT
  )
  add_subdirectory(js-native-api)
endif()
