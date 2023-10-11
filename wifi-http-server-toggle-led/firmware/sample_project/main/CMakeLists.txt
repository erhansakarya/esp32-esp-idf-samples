idf_component_register(SRCS "main.c" "toggle_led.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES
                        "../certs/amazon.crt"
                    )
