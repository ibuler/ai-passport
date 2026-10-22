idf_component_register(
    SRCS "main.c" "pocket/model.c" "pocket/store.c" "pocket/network.c"
         "pocket/player.c" "pocket/feeds.c" "pocket/github.c" "pocket/ui.c"
         "pocket/xiaozhi.c" "pocket/xiaozhi_model.c" "pocket/xiaozhi_protocol.c" "pocket/furina.c" "../assets/fonts/pocket_font_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_netif esp_wifi esp_timer nvs_flash spiffs
             esp_http_server esp_http_client mbedtls json esp_audio_codec esp_websocket_client mqtt
    EMBED_FILES "settings.html.gz" "../assets/images/furina/furina.rgb565a8"
)
spiffs_create_partition_image(storage "${CMAKE_CURRENT_LIST_DIR}/../assets/storage" FLASH_IN_PROJECT)
