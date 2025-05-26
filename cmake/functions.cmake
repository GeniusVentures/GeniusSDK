function(disable_clang_tidy target)
  set_target_properties(${target} PROPERTIES
      C_CLANG_TIDY ""
      CXX_CLANG_TIDY ""
      )
endfunction()

function(addtest test_name)
  add_executable(${test_name} ${ARGN})
  addtest_part(${test_name} ${ARGN})
  target_link_libraries(${test_name}
      GTest::gtest_main
      GTest::gmock_main
      )
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/xunit)
  set(xml_output "--gtest_output=xml:${CMAKE_BINARY_DIR}/xunit/xunit-${test_name}.xml")
  add_test(
      NAME ${test_name}
      COMMAND $<TARGET_FILE:${test_name}> ${xml_output}
  )
  set_target_properties(${test_name} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test_bin
      ARCHIVE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/test_lib
      LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/test_lib
      )
  disable_clang_tidy(${test_name})

  if(FORCE_MULTILE)
    set_target_properties(${test_name} PROPERTIES LINK_FLAGS "${MULTIPLE_OPTION}")
  endif()
endfunction()

function(addtest_part test_name)
  if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
  endif ()
  target_sources(${test_name} PUBLIC
      ${ARGN}
      )
  target_link_libraries(${test_name}
      GTest::gtest
      )
endfunction()

# conditionally applies flag.
function(add_flag flag)
  check_cxx_compiler_flag(${flag} FLAG_${flag})
  if (FLAG_${flag} EQUAL 1)
    add_compile_options(${flag})
  endif ()
endfunction()

function(print)
  message(STATUS "[${CMAKE_PROJECT_NAME}] ${ARGV}")
endfunction()

# geniussdk_install should be called right after add_library(target)
function(geniussdk_install target)
    install(TARGETS ${target} EXPORT GeniusSDKTargets
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME       DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FRAMEWORK     DESTINATION ${CMAKE_INSTALL_PREFIX}
        BUNDLE        DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
endfunction()


function(compile_proto_to_cpp_sdk PB_H PB_CC PB_REL_PATH PROTO)
  get_target_property(Protobuf_INCLUDE_DIR protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(Protobuf_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)
  find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)

  if (NOT Protobuf_PROTOC_EXECUTABLE)
    message(FATAL_ERROR "Protobuf_PROTOC_EXECUTABLE is empty")
  endif ()
  if (NOT Protobuf_INCLUDE_DIR)
    message(FATAL_ERROR "Protobuf_INCLUDE_DIR is empty")
  endif ()
  if (NOT GRPC_CPP_PLUGIN_EXECUTABLE)
    message(FATAL_ERROR "grpc_cpp_plugin not found in PATH")
  endif ()

  get_filename_component(PROTO_ABS "${PROTO}" REALPATH)

  # Determine schema output directory
  file(RELATIVE_PATH SCHEMA_REL "${CMAKE_BINARY_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}")
  set(SCHEMA_OUT_DIR ${CMAKE_BINARY_DIR}/generated)
  file(MAKE_DIRECTORY ${SCHEMA_OUT_DIR})

  # Filenames
  string(REGEX REPLACE "\\.proto$" ".pb.h" GEN_PB_HEADER ${PROTO})
  string(REGEX REPLACE "\\.proto$" ".pb.cc" GEN_PB ${PROTO})
  string(REGEX REPLACE "\\.proto$" ".grpc.pb.h" GEN_GRPC_HEADER ${PROTO})
  string(REGEX REPLACE "\\.proto$" ".grpc.pb.cc" GEN_GRPC ${PROTO})

  # Full paths to outputs
  set(GEN_PB_HEADER_PATH ${SCHEMA_OUT_DIR}/${SCHEMA_REL}/${GEN_PB_HEADER})
  set(GEN_PB_PATH        ${SCHEMA_OUT_DIR}/${SCHEMA_REL}/${GEN_PB})
  set(GEN_GRPC_HEADER_PATH ${SCHEMA_OUT_DIR}/${SCHEMA_REL}/${GEN_GRPC_HEADER})
  set(GEN_GRPC_PATH        ${SCHEMA_OUT_DIR}/${SCHEMA_REL}/${GEN_GRPC})

  add_custom_command(
      OUTPUT ${GEN_PB_HEADER_PATH} ${GEN_PB_PATH} ${GEN_GRPC_HEADER_PATH} ${GEN_GRPC_PATH}
      COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      ARGS -I${PROJECT_ROOT}/src -I${Protobuf_INCLUDE_DIR}
           --cpp_out=${SCHEMA_OUT_DIR}
           --grpc_out=${SCHEMA_OUT_DIR}
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
           ${PROTO_ABS}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS protobuf::protoc
      VERBATIM
  )

  set(${PB_H} "${GEN_PB_HEADER_PATH};${GEN_GRPC_HEADER_PATH}" PARENT_SCOPE)
  set(${PB_CC} "${GEN_PB_PATH};${GEN_GRPC_PATH}" PARENT_SCOPE)
  set(${PB_REL_PATH} ${SCHEMA_REL} PARENT_SCOPE)
endfunction()

if(NOT TARGET generated)
add_custom_target(generated
    COMMENT "Building generated files..."
    )
endif()

function(add_proto_library_sdk NAME)
  set(SOURCES "")
  set(HEADERS "")
  set(PB_REL_PATH "")
  foreach (PROTO IN ITEMS ${ARGN})
    compile_proto_to_cpp_sdk(H C PB_REL_PATH ${PROTO})
    list(APPEND SOURCES ${H} ${C})
    list(APPEND HEADERS ${H})
  endforeach ()

  add_library(${NAME}
      ${SOURCES}
      )
  target_link_libraries(${NAME}
      protobuf::libprotobuf
      )
  target_include_directories(${NAME} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated/>
      $<INSTALL_INTERFACE:include>
  )
  #target_include_directories(${NAME} PUBLIC
  #    ${CMAKE_BINARY_DIR}/generated/
  #    )
  foreach (H IN ITEMS ${HEADERS})
    set_target_properties(${NAME} PROPERTIES PUBLIC_HEADER "${H}")
  endforeach ()

  install(TARGETS ${NAME} EXPORT GeniusSDKTargets
      PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PB_REL_PATH}
      )

  disable_clang_tidy(${NAME})

  add_dependencies(generated ${NAME})
endfunction()

