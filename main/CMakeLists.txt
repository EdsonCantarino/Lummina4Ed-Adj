# ==== Lummina4Ed main component CMakeLists.txt ====
# Refatorado para branding automático
# Default: MAXXIMED
# Pode ser sobrescrito via:
#   idf.py -DLOGO=BIOSTERONS reconfigure
#   idf.py build

idf_component_register(
    SRCS
        "main.cpp"
        "task_manager.cpp"
        "rtc_ds1302.cpp"
        "heater.cpp"
        "heater_fail.cpp"
        "temperature.cpp"
        "led_panel.cpp"
        "i2c_driver.cpp"
        "pcf8574_driver.cpp"
        "keyboard.cpp"
        "buzzer.cpp"
        "ampoule_sensor.cpp"
        "led_uv.cpp"
        "ampoules.cpp"
        "light_sensor.cpp"
        "light_sensor_ads1248.cpp"
        "ampoule_test.cpp"
        "printer.cpp"
        "app_httpd.cpp"
        "http_utils.cpp"
        "nvs_utils.cpp"
        "json.cpp"
        "nvs_sync.cpp"
        "wifi_ap.c"
        "helper_utils.cpp"
        "security.cpp"
        "coredump_to_server.cpp"
        "http_app.c"
        "branding.cpp"
    INCLUDE_DIRS "."
)

# -----------------------------------------------------------------------------
# Branding selection
# -----------------------------------------------------------------------------
# Valor default (fica no cache do CMake)
# Pode ser sobrescrito com -DLOGO=XXXX no idf.py reconfigure
# -----------------------------------------------------------------------------
set(LOGO "MAXXIMED" CACHE STRING "Branding key")

# -----------------------------------------------------------------------------
# Exporta LOGO como string literal para o C/C++
# Exemplo final no compilador:
#   -DLOGO=\"MAXXIMED\"
# -----------------------------------------------------------------------------
target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
        LOGO=\"${LOGO}\"
)
