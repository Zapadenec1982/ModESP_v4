idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/generated"
    PRIV_REQUIRES modesp_core modesp_services modesp_hal modesp_net modesp_mqtt equipment protection thermostat
)
