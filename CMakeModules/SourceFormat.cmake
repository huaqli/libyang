cmake_minimum_required(VERSION 2.8.12)

# format source files with clang-format, at least version 10 required
macro(LIBYANG_FORMAT)
    find_program(CLANG_FORMAT NAMES clang-format-11.0.0 clang-format-10.0.1 clang-format-10.0.0)
    find_package_handle_standard_args(clang-format DEFAULT_MSG CLANG_FORMAT)

    if(CLANG_FORMAT)
        add_custom_target(format
                COMMAND ${CLANG_FORMAT} -style=file -i ${CMAKE_SOURCE_DIR}/src/*.[hc] ${CMAKE_SOURCE_DIR}/compat/*.[hc]
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Formating sources with ${CLANG_FORMAT} ...")
    endif()
endmacro()
