add_definitions (
    -DHAVE_STDLIB_H
    -DHAVE_STRING_H
)

add_library (libiberty STATIC
    cp-demangle.c
)
