# contrib/ai/examples/nr-traces-ai/CMakeLists.txt

build_lib_example(
    NAME nr-traces-ai
    SOURCE_FILES nr-traces-ai.cc
    LIBRARIES_TO_LINK ${libai} ${libnr}
)

pybind11_add_module(nr_traces_ai_binding nr-traces-ai-py.cc)
set_target_properties(nr_traces_ai_binding PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(nr-traces-ai nr_traces_ai_binding)
