idf_component_register(

    SRCS
        "display.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        "util"
        "lcd_gc9a01"
        "image"
    )

# set_target_properties(${COMPONENT_LIB} PROPERTIES COMPILE_FLAGS "-save-temps=obj")
