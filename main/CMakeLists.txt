# Get IDF_PATH with forward slashes to avoid Windows backslash escape issues
file(TO_CMAKE_PATH "$ENV{IDF_PATH}" IDF_PATH_CMAKE)
idf_component_register(SRCS CommandHandler.cpp http_client.c sketch.ino.cpp
                      REQUIRES bt esp_http_client spiffs SPIS boards
                      PRIV_REQUIRES nvs_flash
                      PRIV_INCLUDE_DIRS "."
                      INCLUDE_DIRS "." "${IDF_PATH_CMAKE}/components/esp_netif"
)
