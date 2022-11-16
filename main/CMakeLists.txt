idf_component_register(SRCS "esp32-demo-main.c" "appmqtt.c" "appwebserver.c" "appota.c" "appstate.c" "appwifi.c" "appfilesystem.c" "frozen/frozen.c" "appsensors.c" "PmodOLEDrgb.c" "BME280_driver/bme280.c" "appbme280.c" "appultrasonic.c" "appuart.c"
	INCLUDE_DIRS "." "./frozen"  "./BME280_driver"
                       EMBED_TXTFILES ${project_dir}/ca/caroot.pem ${project_dir}/ca/cakey.pem)
	       
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)

