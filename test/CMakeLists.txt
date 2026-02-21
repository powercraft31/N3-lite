idf_component_register(
    SRCS
        "test_allocation_controller.c"
        "MockMeterImpl.c"
        "stubs/stub_cevent.c"
        "stubs/stub_gpio_manager.c"
        "stubs/stub_charging_station_mgr.c"
        "stubs/stub_misc.c"
        "stubs/stub_freertos.c"
        "../main/AllocationController.c"
    INCLUDE_DIRS
        "."
        "stubs"
        "../main"
    PRIV_REQUIRES
        unity
)

# Define UNIT_TEST so AllocationController.c exposes test hooks
target_compile_definitions(${COMPONENT_LIB} PRIVATE UNIT_TEST)
# portTICK_PERIOD_MS=1 makes tick arithmetic predictable in tests
target_compile_definitions(${COMPONENT_LIB} PRIVATE portTICK_PERIOD_MS=1)
