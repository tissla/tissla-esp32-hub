idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
	REQUIRES wifi led http nvs_flash
)
