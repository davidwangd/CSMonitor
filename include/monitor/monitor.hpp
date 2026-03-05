#pragma once

#include <string>
#include <vector>

namespace process_monitor {

void run_monitor(int tick_interval_ms = 1000);

}

std::vector<std::string> get_job_list();
std::string get_job_stdout_file(const std::string& job);
std::string get_job_stderr_file(const std::string& job);
std::string get_job_info(const std::string& job);