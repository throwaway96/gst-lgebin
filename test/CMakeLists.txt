include_directories (${lgebin_SOURCE_DIR}/include)
link_directories(${lgebin_BINARY_DIR}/src)

set(INSTANCE_OBJS test-registry)
add_executable(test-registry ${INSTANCE_OBJS})
target_link_libraries(test-registry 
	lgebin
	${GLIB2_LIBRARIES}
	${GOBJECT_LIBRARIES}
	${GST_LIBRARIES}	
	${GSTBASE_LIBRARIES}
	)
	