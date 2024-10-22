idf_component_register(SRCS "main.c" "usb_hid.c" "hid_ev.c" "audio.c" "cd_cue.c"
					"eth_connect.c" "font_8x16.c"
					"libusbhid/descr.c" "libusbhid/parse.c"
					"libusbhid/usage_tables.c" "libusbhid/data.c"
                    INCLUDE_DIRS "."
					PRIV_INCLUDE_DIRS "libusbhid")

#hack: otherwise audio.c is not linked
#should actually factor refactor all things called by quake into a separate component
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u snd_inited -u CDAudio_Init")