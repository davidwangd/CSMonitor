add_executable(example_monitor example_monitor.cpp)

target_link_libraries(example_monitor
    PRIVATE
        monitor
        ftxui::component
)

add_executable(duonao_monitor duonao_monitor.cpp)

target_link_libraries(duonao_monitor
    PRIVATE
        monitor
        ftxui::component
)