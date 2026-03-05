#include "monitor/monitor.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static std::string example_dir = "/tmp/process_monitor_example";
static std::atomic<bool> running_jobs{true};
static std::thread job_thread;

static std::vector<std::string> jobs = {"web_server", "database", "worker_1", "worker_2"};
static std::unordered_map<std::string, int> job_counters;

std::vector<std::string> get_job_list() {
    return jobs;
}

std::string get_job_stdout_file(const std::string& job) {
    return example_dir + "/" + job + ".out";
}

std::string get_job_stderr_file(const std::string& job) {
    return example_dir + "/" + job + ".err";
}

std::string get_job_info(const std::string& job) {
    int counter = job_counters[job];
    return job + " | Status: running | Log count: " + std::to_string(counter);
}

void simulate_job_output() {
    fs::create_directories(example_dir);

    for (const auto& job : jobs) {
        job_counters[job] = 0;
        std::ofstream out(example_dir + "/" + job + ".out");
        std::ofstream err(example_dir + "/" + job + ".err");
        out << "[" << job << "] Starting up...\n";
        err << "[" << job << "] No errors yet\n";
    }

    while (running_jobs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        for (const auto& job : jobs) {
            job_counters[job]++;

            std::ofstream out(example_dir + "/" + job + ".out", std::ios::app);
            out << "[" << job << "] Log entry #" << job_counters[job] << "\n";

            if (job_counters[job] % 5 == 0) {
                std::ofstream err(example_dir + "/" + job + ".err", std::ios::app);
                err << "[" << job << "] Warning: something happened at #" << job_counters[job] << "\n";
            }
        }
    }
}

void cleanup() {
    running_jobs = false;
    if (job_thread.joinable()) {
        job_thread.join();
    }
    fs::remove_all(example_dir);
}

int main() {
    std::cout << "Process Monitor Example\n";
    std::cout << "=======================\n\n";
    std::cout << "This example simulates running jobs with dynamic output.\n";
    std::cout << "Jobs will produce new log entries every 500ms.\n\n";
    std::cout << "Controls:\n";
    std::cout << "  - Left/Right arrows or 'h'/'l': Navigate between jobs\n";
    std::cout << "  - 'q' or Escape: Quit\n\n";
    std::cout << "Jobs being monitored:\n";
    for (const auto& job : jobs) {
        std::cout << "  - " << job << "\n";
    }
    std::cout << "\nStarting monitor...\n\n";

    job_thread = std::thread(simulate_job_output);

    try {
        process_monitor::run_monitor(1000);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        cleanup();
        return 1;
    }

    cleanup();
    std::cout << "Monitor closed. Cleaned up temporary files.\n";

    return 0;
}