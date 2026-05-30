# Get IDF_PATH with forward slashes to avoid Windows backslash escape issues
file(TO_CMAKE_PATH "$ENV{IDF_PATH}" IDF_PATH_CMAKE)

set(_main_srcs CommandHandler.cpp stream_handler.cpp http_client.c sketch.ino.cpp)
set(_mqtt_ca_crt "${CMAKE_CURRENT_SOURCE_DIR}/../certs/ca.crt")

if(EXISTS "${_mqtt_ca_crt}")
  idf_component_register(SRCS ${_main_srcs}
                        REQUIRES bt esp_http_client spiffs SPIS boards
                        PRIV_REQUIRES nvs_flash
                        PRIV_INCLUDE_DIRS "."
                        INCLUDE_DIRS "." "${IDF_PATH_CMAKE}/components/esp_netif"
                        EMBED_TXTFILES "../certs/ca.crt")
  target_compile_definitions(${COMPONENT_LIB} PUBLIC NINA_MQTT_CA_EMBED=1)
else()
  idf_component_register(SRCS ${_main_srcs}
                        REQUIRES bt esp_http_client spiffs SPIS boards
                        PRIV_REQUIRES nvs_flash
                        PRIV_INCLUDE_DIRS "."
                        INCLUDE_DIRS "." "${IDF_PATH_CMAKE}/components/esp_netif")
endif()
