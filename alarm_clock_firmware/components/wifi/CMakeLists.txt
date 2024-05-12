idf_component_register(SRCS "wifi.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES "wifi_provisioning" "lcd_gc9a01" "display" "espressif__qrcode" "util" "ui"
                    )
