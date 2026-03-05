add_executable(example_monitor example_monitor.cpp)

target_link_libraries(example_monitor
    PRIVATE
        monitor
        ftxui::component
)