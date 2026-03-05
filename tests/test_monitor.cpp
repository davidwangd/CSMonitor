#include "monitor/monitor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static std::string test_dir;

std::vector<std::string> get_job_list() {
    return {"job_alpha", "job_beta", "job_gamma"};
}

std::string get_job_stdout_file(const std::string& job) {
    return test_dir + "/" + job + ".stdout";
}

std::string get_job_stderr_file(const std::string& job) {
    return test_dir + "/" + job + ".stderr";
}

std::string get_job_info(const std::string& job) {
    return "Job: " + job + " | Status: completed";
}

void create_test_files() {
    test_dir = "/tmp/process_monitor_test";
    fs::create_directories(test_dir);

    for (const auto& job : get_job_list()) {
        std::ofstream stdout_file(test_dir + "/" + job + ".stdout");
        stdout_file << "=== " << job << " stdout ===\n";
        stdout_file << "Starting job " << job << "...\n";
        stdout_file << "Processing data...\n";
        stdout_file << "Job " << job << " completed.\n";

        std::ofstream stderr_file(test_dir + "/" + job + ".stderr");
        stderr_file << "=== " << job << " stderr ===\n";
        stderr_file << "Warning: minor issue in " << job << "\n";
    }
}

void cleanup_test_files() {
    fs::remove_all(test_dir);
}

int main() {
    std::cout << "Creating test files...\n";
    create_test_files();

    std::cout << "Starting monitor with test jobs:\n";
    for (const auto& job : get_job_list()) {
        std::cout << "  - " << job << "\n";
        std::cout << "    stdout: " << get_job_stdout_file(job) << "\n";
        std::cout << "    stderr: " << get_job_stderr_file(job) << "\n";
    }
    std::cout << "\nPress 'q' or Escape to quit.\n";
    std::cout << "Use left/right arrows or 'h'/'l' to navigate between jobs.\n\n";

    try {
        process_monitor::run_monitor(1000);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        cleanup_test_files();
        return 1;
    }

    std::cout << "\nCleaning up test files...\n";
    cleanup_test_files();

    return 0;
}