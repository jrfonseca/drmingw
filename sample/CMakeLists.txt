force_debug ()

add_executable (sample
    sample.cpp
    exchndl2.cpp
)

install (FILES sample.cpp exchndl2.cpp DESTINATION sample)
install (FILES sample.mak DESTINATION sample RENAME Makefile)
