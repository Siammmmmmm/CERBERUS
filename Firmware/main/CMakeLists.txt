idf_component_register(SRCS "store.c" "main.c" "transport.c"
                       INCLUDE_DIRS "." "include" "../../Common"
                       REQUIRES nvs_flash esp_driver_gpio esp_driver_uart)