#include "monitor/monitor.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static std::string example_dir = "/tmp/process_monitor_example";
static std::atomic<bool> running_jobs{true};
static std::thread job_thread;
static std::thread job_mgmt_thread;
static std::mutex jobs_mutex;

static std::vector<std::string> jobs = {"web_server", "database", "worker_1", "worker_2"};
static std::unordered_map<std::string, int> job_counters;
static int next_worker_id = 3;

std::vector<std::string> get_job_list() {
    std::lock_guard<std::mutex> lock(jobs_mutex);
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

void manage_jobs() {
    while (running_jobs) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        if (!running_jobs) break;
        
        std::lock_guard<std::mutex> lock(jobs_mutex);
        
        std::string new_job = "worker_" + std::to_string(next_worker_id++);
        jobs.push_back(new_job);
        job_counters[new_job] = 0;
        
        std::ofstream out(example_dir + "/" + new_job + ".out");
        std::ofstream err(example_dir + "/" + new_job + ".err");
        out << "[" << new_job << "] Starting up...\n";
        err << "[" << new_job << "] No errors yet\n";
        
        if (jobs.size() > 1) {
            size_t random_idx = std::rand() % jobs.size();
            std::string removed_job = jobs[random_idx];
            jobs.erase(jobs.begin() + random_idx);
            job_counters.erase(removed_job);
        }
    }
}

void simulate_job_output() {
    fs::create_directories(example_dir);

    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        for (const auto& job : jobs) {
            job_counters[job] = 0;
            std::ofstream out(example_dir + "/" + job + ".out");
            std::ofstream err(example_dir + "/" + job + ".err");
            out << "[" << job << "] Starting up...\n";
            err << "[" << job << "] No errors yet\n";
        }
    }

    while (running_jobs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::lock_guard<std::mutex> lock(jobs_mutex);
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
    if (job_mgmt_thread.joinable()) {
        job_mgmt_thread.join();
    }
    fs::remove_all(example_dir);
}

int main() {
    std::srand(std::time(nullptr));
    
    std::cout << "Process Monitor Example\n";
    std::cout << "=======================\n\n";
    std::cout << "This example simulates running jobs with dynamic output.\n";
    std::cout << "Jobs will produce new log entries every 500ms.\n";
    std::cout << "Every 30 seconds, a new job is added and a random job is removed.\n\n";
    std::cout << "Controls:\n";
    std::cout << "  - Left/Right arrows or 'h'/'l': Navigate between jobs\n";
    std::cout << "  - Tab: Switch between stdout and stderr panes\n";
    std::cout << "  - 'q' or Escape: Quit\n\n";
    std::cout << "Jobs being monitored:\n";
    for (const auto& job : jobs) {
        std::cout << "  - " << job << "\n";
    }
    std::cout << "\nStarting monitor...\n\n";

    job_thread = std::thread(simulate_job_output);
    job_mgmt_thread = std::thread(manage_jobs);

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