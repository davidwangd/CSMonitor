#include "monitor/monitor.hpp"
#include "file_watcher.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace process_monitor {

using namespace ftxui;

struct JobState {
    std::string name;
    std::string stdout_content;
    std::string stderr_content;
    bool stdout_changed = false;
    bool stderr_changed = false;
};

class MonitorImpl {
public:
    MonitorImpl(int tick_interval_ms)
        : tick_interval_ms_(tick_interval_ms)
        , selected_index_(0)
        , stdout_scroll_(0)
        , stderr_scroll_(0)
        , running_(true) {}

    void run() {
        auto screen = ScreenInteractive::Fullscreen();

        job_list_ = get_job_list();
        if (!job_list_.empty()) {
            selected_index_ = 0;
            update_selected_job();
        }

        auto renderer = create_renderer();

        std::thread update_thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms_));
                update_jobs();
            }
        });

        screen.Loop(renderer);
        running_ = false;
        update_thread.join();
    }

private:
    Component create_renderer() {
        auto container = Container::Vertical({});

        auto renderer = Renderer(container, [this]() {
            update_selected_job();

            Elements job_buttons;
            for (size_t i = 0; i < job_list_.size(); ++i) {
                const auto& job = job_list_[i];
                auto it = job_states_.find(job);
                bool has_update = false;
                if (it != job_states_.end()) {
                    has_update = it->second.stdout_changed || it->second.stderr_changed;
                }

                Element job_elem = text(job);

                if (i == selected_index_) {
                    job_elem = job_elem | inverted;
                } else if (has_update) {
                    job_elem = job_elem | bgcolor(Color::Yellow);
                }

                job_buttons.push_back(job_elem);
            }

            Elements nav_elements;
            for (size_t i = 0; i < job_buttons.size(); ++i) {
                nav_elements.push_back(job_buttons[i]);
                if (i + 1 < job_buttons.size()) {
                    nav_elements.push_back(text("  "));
                }
            }
            auto navbar = hbox(nav_elements) | border;

            auto stdout_content = text(current_stdout_);
            auto stderr_content = text(current_stderr_);

            if (!current_stdout_.empty()) {
                stdout_content = vbox({
                    text("=== STDOUT ===") | bold,
                    paragraph(current_stdout_),
                });
            } else {
                stdout_content = text("(No stdout)") | dim;
            }

            if (!current_stderr_.empty()) {
                stderr_content = vbox({
                    text("=== STDERR ===") | bold,
                    paragraph(current_stderr_),
                });
            } else {
                stderr_content = text("(No stderr)") | dim;
            }

            auto left_pane = stdout_content | flex | frame | border;
            auto right_pane = stderr_content | flex | frame | border;

            auto main_view = hbox({
                left_pane,
                separator(),
                right_pane,
            });

            Elements layout_elements;
            layout_elements.push_back(navbar);
            layout_elements.push_back(separator());

            if (!current_info_.empty()) {
                layout_elements.push_back(text(current_info_) | border);
                layout_elements.push_back(separator());
            }

            layout_elements.push_back(main_view | flex);

            return vbox(layout_elements);
        });

        renderer |= CatchEvent([this](Event event) {
            if (event == Event::ArrowLeft || event == Event::Character('h')) {
                if (selected_index_ > 0) {
                    --selected_index_;
                    clear_change_flags();
                }
                return true;
            }
            if (event == Event::ArrowRight || event == Event::Character('l')) {
                if (selected_index_ + 1 < job_list_.size()) {
                    ++selected_index_;
                    clear_change_flags();
                }
                return true;
            }
            if (event == Event::ArrowUp || event == Event::Character('k')) {
                return true;
            }
            if (event == Event::ArrowDown || event == Event::Character('j')) {
                return true;
            }
            if (event == Event::Character('q') || event == Event::Escape) {
                running_ = false;
                return false;
            }
            return false;
        });

        return renderer;
    }

    void update_jobs() {
        auto new_jobs = get_job_list();

        std::unordered_set<std::string> new_job_set(new_jobs.begin(), new_jobs.end());

        std::vector<std::string> to_remove;
        for (const auto& [job, state] : job_states_) {
            if (new_job_set.find(job) == new_job_set.end()) {
                to_remove.push_back(job);
            }
        }
        for (const auto& job : to_remove) {
            job_states_.erase(job);
        }

        job_list_ = new_jobs;

        for (const auto& job : job_list_) {
            auto stdout_file = get_job_stdout_file(job);
            auto stderr_file = get_job_stderr_file(job);

            auto& state = job_states_[job];
            state.name = job;

            if (!stdout_file.empty()) {
                auto info = file_watcher_.get_file_info(stdout_file);
                if (info) {
                    state.stdout_content = info->content;
                    state.stdout_changed = info->has_changed;
                }
            }

            if (!stderr_file.empty()) {
                auto info = file_watcher_.get_file_info(stderr_file);
                if (info) {
                    state.stderr_content = info->content;
                    state.stderr_changed = info->has_changed;
                }
            }
        }
    }

    void update_selected_job() {
        if (job_list_.empty() || selected_index_ >= job_list_.size()) {
            current_stdout_.clear();
            current_stderr_.clear();
            current_info_.clear();
            return;
        }

        const auto& job = job_list_[selected_index_];
        current_info_ = get_job_info(job);

        auto it = job_states_.find(job);
        if (it != job_states_.end()) {
            current_stdout_ = it->second.stdout_content;
            current_stderr_ = it->second.stderr_content;
        } else {
            auto stdout_file = get_job_stdout_file(job);
            auto stderr_file = get_job_stderr_file(job);

            if (!stdout_file.empty()) {
                auto info = file_watcher_.get_file_info(stdout_file);
                current_stdout_ = info ? info->content : "";
            }
            if (!stderr_file.empty()) {
                auto info = file_watcher_.get_file_info(stderr_file);
                current_stderr_ = info ? info->content : "";
            }
        }
    }

    void clear_change_flags() {
        if (selected_index_ >= job_list_.size()) return;
        const auto& job = job_list_[selected_index_];
        auto it = job_states_.find(job);
        if (it != job_states_.end()) {
            it->second.stdout_changed = false;
            it->second.stderr_changed = false;
        }
    }

    int tick_interval_ms_;
    size_t selected_index_;
    int stdout_scroll_;
    int stderr_scroll_;
    std::atomic<bool> running_;

    std::vector<std::string> job_list_;
    std::unordered_map<std::string, JobState> job_states_;
    std::string current_stdout_;
    std::string current_stderr_;
    std::string current_info_;

    FileWatcher file_watcher_;
};

void run_monitor(int tick_interval_ms) {
    MonitorImpl monitor(tick_interval_ms);
    monitor.run();
}

}