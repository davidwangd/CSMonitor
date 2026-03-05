add_executable(test_monitor test_monitor.cpp)

target_link_libraries(test_monitor
    PRIVATE
        monitor
        ftxui::component
)

add_test(NAME test_monitor COMMAND test_monitor)