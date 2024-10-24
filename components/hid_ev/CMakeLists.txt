idf_component_register(SRCS "hid_ev.c" 
					"libusbhid/descr.c" "libusbhid/parse.c"
					"libusbhid/usage_tables.c" "libusbhid/data.c"
                    INCLUDE_DIRS "."
					PRIV_INCLUDE_DIRS "libusbhid")

