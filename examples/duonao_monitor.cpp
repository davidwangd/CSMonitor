#include "monitor/monitor.hpp"

#include <array>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

static std::string trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && (str[start] == ' ' || str[start] == '\t')) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t')) {
        --end;
    }
    return str.substr(start, end - start);
}

static std::vector<std::string> execute_command(const std::string& cmd) {
    std::vector<std::string> lines;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return lines;
    }
    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        std::string line = buffer.data();
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        line = trim(line);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    pclose(pipe);
    return lines;
}

std::vector<std::string> get_job_list() {
    return execute_command("djob | grep RUNNING | awk '{ print $1 }'");
}

std::string get_job_stdout_file(const std::string& job) {
    auto lines = execute_command("djob -ll " + job + " | grep stdoutRedirectPath | awk '{ print $2 }'");
    return lines.empty() ? "" : lines[0];
}

std::string get_job_stderr_file(const std::string& job) {
    auto lines = execute_command("djob -ll " + job + " | grep stderrRedirectPath | awk '{ print $2 }'");
    return lines.empty() ? "" : lines[0];
}

std::string get_job_info(const std::string& job) {
    auto lines = execute_command("djob -ll " + job + " | grep name | awk '{ print $2 }'");
    return lines.empty() ? job : lines[0];
}

int main(int argc, char* argv[]) {
    int tick_interval_ms = 1000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-t" || arg == "--tick") && i + 1 < argc) {
            tick_interval_ms = std::stoi(argv[++i]);
        }
    }

    process_monitor::run_monitor(tick_interval_ms);
    return 0;
}