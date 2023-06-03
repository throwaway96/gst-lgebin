include_directories (${lgebin_SOURCE_DIR}/include)

set(OBJS lgebin lgselectorbin dishsrcbin)

add_library(lgebin SHARED ${OBJS})

target_link_libraries(lgebin 
	${GLIB_LIBRARIES}
	${GOBJECT_LIBRARIES}
	${GST_LIBRARIES}
	${GSTBASE_LIBRARIES}
	${URIPARSER_LIBRARIES}
	)
