include_guard(GLOBAL)

include(ExternalProject)

set(DOGGO_COOK_EXECUTABLE "" CACHE FILEPATH
        "Optional prebuilt native doggo-cook executable; normally built automatically")
set(DOGGO_COOK_SOURCE_DIR "" CACHE PATH
        "DoggoCook source directory; defaults to Tools/DoggoCook in the workspace")
set(DOGGO_HOST_CXX_COMPILER "" CACHE FILEPATH
        "Native host C++ compiler used to build DoggoCook from a Switch cross-build")

function(_doggo_ensure_cooker)
    if (TARGET doggo_cook_host)
        return()
    endif ()

    if (DOGGO_COOK_EXECUTABLE)
        get_filename_component(_doggo_cook_executable "${DOGGO_COOK_EXECUTABLE}" ABSOLUTE)
        if (NOT EXISTS "${_doggo_cook_executable}")
            message(FATAL_ERROR "DOGGO_COOK_EXECUTABLE does not exist: ${_doggo_cook_executable}")
        endif ()

        add_executable(doggo_cook_host IMPORTED GLOBAL)
        set_target_properties(doggo_cook_host PROPERTIES
                IMPORTED_LOCATION "${_doggo_cook_executable}"
        )
        return()
    endif ()

    if (DOGGO_COOK_SOURCE_DIR)
        get_filename_component(_doggo_cook_source "${DOGGO_COOK_SOURCE_DIR}" ABSOLUTE)
    else ()
        get_filename_component(_doggo_cook_source
                "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../Tools/DoggoCook"
                ABSOLUTE
        )
    endif ()
    if (NOT EXISTS "${_doggo_cook_source}/CMakeLists.txt")
        message(FATAL_ERROR
                "DoggoCook source was not found at ${_doggo_cook_source}. Set "
                "DOGGO_COOK_SOURCE_DIR or DOGGO_COOK_EXECUTABLE explicitly."
        )
    endif ()

    if (DOGGO_HOST_CXX_COMPILER)
        get_filename_component(_doggo_host_cxx "${DOGGO_HOST_CXX_COMPILER}" ABSOLUTE)
    else ()
        find_program(_doggo_host_cxx NAMES c++ g++ clang++)
    endif ()
    if (NOT _doggo_host_cxx)
        message(FATAL_ERROR
                "A native host C++ compiler is required to build DoggoCook. "
                "Set DOGGO_HOST_CXX_COMPILER to the compiler used by CLion/WSL."
        )
    endif ()
    if (_doggo_host_cxx MATCHES "aarch64-none-elf")
        message(FATAL_ERROR
                "DOGGO_HOST_CXX_COMPILER resolved to the Switch cross-compiler. "
                "Select a native compiler such as /usr/bin/g++."
        )
    endif ()

    set(_doggo_cook_binary "${CMAKE_BINARY_DIR}/host-tools/doggo-cook")
    if (CMAKE_HOST_WIN32)
        set(_doggo_cook_executable "${_doggo_cook_binary}/doggo-cook.exe")
    else ()
        set(_doggo_cook_executable "${_doggo_cook_binary}/doggo-cook")
    endif ()

    ExternalProject_Add(doggo_cook_host_build
            SOURCE_DIR "${_doggo_cook_source}"
            BINARY_DIR "${_doggo_cook_binary}"
            CMAKE_GENERATOR "${CMAKE_GENERATOR}"
            CMAKE_ARGS
            "-DCMAKE_BUILD_TYPE:STRING=Release"
            "-DCMAKE_CXX_COMPILER:FILEPATH=${_doggo_host_cxx}"
            "-DDOGGO_COOK_BUILD_TESTS:BOOL=OFF"
            BUILD_COMMAND
            "${CMAKE_COMMAND}" --build <BINARY_DIR> --target doggo-cook --config Release
            BUILD_BYPRODUCTS "${_doggo_cook_executable}"
            INSTALL_COMMAND ""
            UPDATE_COMMAND ""
    )

    add_executable(doggo_cook_host IMPORTED GLOBAL)
    set_target_properties(doggo_cook_host PROPERTIES
            IMPORTED_LOCATION "${_doggo_cook_executable}"
    )
    add_dependencies(doggo_cook_host doggo_cook_host_build)
endfunction()

function(doggo_add_shader_abi)
    cmake_parse_arguments(PARSE_ARGV 0 ABI "" "TARGET;ABI;OUTPUT" "")
    if (NOT ABI_TARGET OR NOT ABI_ABI OR NOT ABI_OUTPUT)
        message(FATAL_ERROR "doggo_add_shader_abi requires TARGET, ABI, and OUTPUT")
    endif ()
    if (TARGET "${ABI_TARGET}")
        message(FATAL_ERROR "doggo_add_shader_abi target already exists: ${ABI_TARGET}")
    endif ()

    _doggo_ensure_cooker()
    get_filename_component(ABI_ABI "${ABI_ABI}" ABSOLUTE)
    get_filename_component(ABI_OUTPUT "${ABI_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    add_custom_command(
            OUTPUT "${ABI_OUTPUT}"
            COMMAND "$<TARGET_FILE:doggo_cook_host>"
            shader-abi
            --abi "${ABI_ABI}"
            --output "${ABI_OUTPUT}"
            DEPENDS doggo_cook_host "${ABI_ABI}"
            COMMENT "Generating DOGGO shader binding ABI"
            VERBATIM
    )
    add_custom_target(${ABI_TARGET} DEPENDS "${ABI_OUTPUT}")
endfunction()

function(doggo_add_shader_program)
    cmake_parse_arguments(PARSE_ARGV 0 SHADER ""
            "TARGET;MANIFEST;ABI;ABI_TARGET;OUTPUT_DIRECTORY;UAM"
            "SOURCES;OUTPUTS"
    )
    if (NOT SHADER_TARGET OR NOT SHADER_MANIFEST OR NOT SHADER_ABI OR
            NOT SHADER_OUTPUT_DIRECTORY OR NOT SHADER_UAM OR NOT SHADER_SOURCES OR NOT SHADER_OUTPUTS)
        message(FATAL_ERROR
                "doggo_add_shader_program requires TARGET, MANIFEST, ABI, OUTPUT_DIRECTORY, "
                "UAM, SOURCES, and OUTPUTS"
        )
    endif ()
    if (TARGET "${SHADER_TARGET}")
        message(FATAL_ERROR "doggo_add_shader_program target already exists: ${SHADER_TARGET}")
    endif ()

    _doggo_ensure_cooker()
    get_filename_component(SHADER_MANIFEST "${SHADER_MANIFEST}" ABSOLUTE)
    get_filename_component(SHADER_ABI "${SHADER_ABI}" ABSOLUTE)
    get_filename_component(SHADER_OUTPUT_DIRECTORY "${SHADER_OUTPUT_DIRECTORY}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    set(_doggo_shader_sources "")
    foreach (_source IN LISTS SHADER_SOURCES)
        get_filename_component(_source "${_source}" ABSOLUTE)
        list(APPEND _doggo_shader_sources "${_source}")
    endforeach ()

    set(_doggo_shader_outputs "")
    foreach (_output IN LISTS SHADER_OUTPUTS)
        if (IS_ABSOLUTE "${_output}")
            message(FATAL_ERROR "Shader OUTPUTS must be filenames relative to OUTPUT_DIRECTORY")
        endif ()
        list(APPEND _doggo_shader_outputs "${SHADER_OUTPUT_DIRECTORY}/${_output}")
    endforeach ()

    set(_doggo_shader_dependencies
            doggo_cook_host
            "${SHADER_MANIFEST}"
            "${SHADER_ABI}"
            ${_doggo_shader_sources}
    )
    if (SHADER_ABI_TARGET)
        if (NOT TARGET "${SHADER_ABI_TARGET}")
            message(FATAL_ERROR "Shader ABI target does not exist: ${SHADER_ABI_TARGET}")
        endif ()
        list(APPEND _doggo_shader_dependencies "${SHADER_ABI_TARGET}")
    endif ()

    add_custom_command(
            OUTPUT ${_doggo_shader_outputs}
            COMMAND "$<TARGET_FILE:doggo_cook_host>"
            shader
            --manifest "${SHADER_MANIFEST}"
            --abi "${SHADER_ABI}"
            --output "${SHADER_OUTPUT_DIRECTORY}"
            --uam "${SHADER_UAM}"
            DEPENDS ${_doggo_shader_dependencies}
            COMMENT "Cooking shader program ${SHADER_TARGET}"
            VERBATIM
    )

    add_custom_target(${SHADER_TARGET} DEPENDS ${_doggo_shader_outputs})
    dkp_set_target_file(${SHADER_TARGET} ${_doggo_shader_outputs})
endfunction()
