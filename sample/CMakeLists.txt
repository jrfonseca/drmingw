force_debug ()

add_executable (sample
    sample.cpp
)
target_link_libraries (sample PRIVATE exchndl)

install (FILES sample.cpp DESTINATION sample)
install (FILES sample.mak DESTINATION sample RENAME Makefile)
