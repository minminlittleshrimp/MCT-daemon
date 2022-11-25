add_subdirectory( lib )
add_subdirectory( daemon )

if( WITH_DLT_CONSOLE )
    add_subdirectory( console )
endif( WITH_DLT_CONSOLE )

if( WITH_DLT_EXAMPLES )
    add_subdirectory( examples )
endif( WITH_DLT_EXAMPLES )


