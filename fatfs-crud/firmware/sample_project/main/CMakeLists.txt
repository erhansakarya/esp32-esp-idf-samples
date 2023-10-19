idf_component_register(SRCS "main.c" "toggle_led.c" "push_button.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES
                        "../certs/amazon.crt"
                        "files/index.html"
                        "files/sample.txt"
                    EMBED_FILES
                        "files/pinout.jpg"
                    )

# NOTE: SPIFFS
# spiffs_create_partition_image(storage ../spiffs_dir FLASH_IN_PROJECT)

# NOTE: FAT
fatfs_create_spiflash_image(storage ../spiffs_dir FLASH_IN_PROJECT)
