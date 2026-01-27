build_lib_example(
    NAME nr-ai-minimal
    SOURCE_FILES nr-ai-minimal.cc
    LIBRARIES_TO_LINK ${libai} ${libnr}
)

pybind11_add_module(nr_ai_binding nr-ai-minimal-py.cc)
set_target_properties(nr_ai_binding PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(nr-ai-minimal nr_ai_binding)
