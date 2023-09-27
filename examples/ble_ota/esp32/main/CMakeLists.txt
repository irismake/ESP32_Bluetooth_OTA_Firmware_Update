idf_component_register(SRCS "main.c" "gatt_svr.c" "gap.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt
	            REQUIRES app_update
		    REQUIRES nvs_flash)
