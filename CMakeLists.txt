if(CONFIG_PG1KB_INERTIA_RUNTIME_SETTINGS)
    target_sources(app PRIVATE src/pg1kb_inertia_runtime_settings.c)
endif()

if(CONFIG_PG1KB_BLE_MANAGEMENT_STUDIO_RPC)
    target_sources(app PRIVATE src/pg1kb_ble_management_studio.c)

    list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
    include(nanopb)
    set(NANOPB_GENERATE_CPP_APPEND_PATH TRUE)
    set(NANOPB_GENERATE_CPP_STANDALONE OFF)

    # Keep generated protobuf objects in a Zephyr library rather than adding
    # them directly to app; this matches the custom Studio RPC module pattern.
    zephyr_library()
    file(GLOB_RECURSE PG1KB_BLE_PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)
    nanopb_generate_cpp(pg1kb_ble_proto_srcs pg1kb_ble_proto_hdrs
        RELPATH ${CMAKE_CURRENT_SOURCE_DIR}
        ${PG1KB_BLE_PROTO_FILES})
    target_include_directories(${ZEPHYR_CURRENT_LIBRARY} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
    target_sources(${ZEPHYR_CURRENT_LIBRARY} PRIVATE
        ${pg1kb_ble_proto_srcs}
        ${pg1kb_ble_proto_hdrs})
    target_include_directories(app PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto)
    add_dependencies(app ${ZEPHYR_CURRENT_LIBRARY})
endif()
