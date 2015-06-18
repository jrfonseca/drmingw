force_debug ()

add_executable (sample
    sample.cpp
)
add_dependencies (sample exchndl)
target_link_libraries (sample $<TARGET_FILE:exchndl>)

install (FILES sample.cpp DESTINATION sample)
install (FILES sample.mak DESTINATION sample RENAME Makefile)
