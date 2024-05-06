set(COMPONENT_REQUIRES "font" "image")
set(COMPONENT_SRCS "assets.c")
list(APPEND COMPONENT_ADD_INCLUDEDIRS "include")

set(COMPONENT_EMBED_FILES
    "font/Cascadia0.png"
    "font/Segoe0.png"
    "font/Digits0.png"
    "font/Big0.png"
    "font/Forte0.png"
    "image/test.png"
    "image/face.png"
    "image/blip.png"
    "image/small_blip.png"
    )

register_component()
