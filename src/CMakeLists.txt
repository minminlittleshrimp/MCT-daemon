add_subdirectory( lib )
add_subdirectory( daemon )
add_subdirectory( writer )

if( WITH_DLT_CONSOLE )
    add_subdirectory( console )
endif( WITH_DLT_CONSOLE )


