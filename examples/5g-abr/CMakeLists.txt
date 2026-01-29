build_lib_example(
    NAME 5g-abr-sim
    SOURCE_FILES 
        5g-abr-sim.cc
        dash-client-app.cc
    LIBRARIES_TO_LINK 
        ${libai} 
        ${libnr}
        ${libinternet}
        ${libapplications}
        ${libpoint-to-point}
)
