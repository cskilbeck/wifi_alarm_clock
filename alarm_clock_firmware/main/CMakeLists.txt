set(COMPONENT_SRCS
    main.c
    lcd.cpp
    wifi.cpp
    led.cpp
    stream.c
    pngle.c
    image.cpp
    font.cpp
    )
set(COMPONENT_ADD_INCLUDEDIRS "")

set(COMPONENT_EMBED_FILES "data/font/Cascadia.png" "data/test.png")

register_component()
