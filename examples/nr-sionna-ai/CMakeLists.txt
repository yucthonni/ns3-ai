# contrib/ai/examples/nr-sionna-ai/CMakeLists.txt

build_lib_example(
    NAME nr-sionna-ai
    SOURCE_FILES nr-sionna-ai.cc
    LIBRARIES_TO_LINK ${libai} ${libnr}
)

pybind11_add_module(nr_sionna_ai_binding nr-sionna-ai-py.cc)
set_target_properties(nr_sionna_ai_binding PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(nr-sionna-ai nr_sionna_ai_binding)
