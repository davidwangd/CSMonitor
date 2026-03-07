#include "monitor/monitor.hpp"
#include "file_watcher.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace process_monitor {

using namespace ftxui;

static std::vector<std::string> split_lines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

static std::vector<std::string> wrap_line(const std::string& line, int width) {
    if (width <= 0 || line.empty()) {
        return {line};
    }
    
    std::vector<std::string> wrapped_lines;
    int line_width = string_width(line);
    
    if (line_width <= width) {
        wrapped_lines.push_back(line);
        return wrapped_lines;
    }
    
    std::string current_line;
    int current_width = 0;
    
    for (char c : line) {
        int char_width = string_width(std::string(1, c));
        if (current_width + char_width > width && !current_line.empty()) {
            wrapped_lines.push_back(current_line);
            current_line.clear();
            current_width = 0;
        }
        current_line += c;
        current_width += char_width;
    }
    
    if (!current_line.empty()) {
        wrapped_lines.push_back(current_line);
    }
    
    return wrapped_lines;
}

static std::vector<std::string> wrap_lines(const std::vector<std::string>& lines, int width) {
    std::vector<std::string> result;
    for (const auto& line : lines) {
        auto wrapped = wrap_line(line, width);
        for (const auto& wl : wrapped) {
            result.push_back(wl);
        }
    }
    return result;
}

struct JobState {
    std::string name;
    std::string info;
    std::string stdout_file;
    std::string stderr_file;
    std::string stdout_content;
    std::string stderr_content;
    bool stdout_changed = false;
    bool stderr_changed = false;
    bool finished = false;
    bool viewed = false;
    bool initialized = false;
};

class MonitorImpl {
public:
    MonitorImpl(int tick_interval_ms, int wrap_width)
        : tick_interval_ms_(tick_interval_ms)
        , wrap_width_(wrap_width)
        , selected_index_(0)
        , stdout_focus_line_(0)
        , stderr_focus_line_(0)
        , stdout_pane_focused_(true)
        , running_(true) {}

    void run() {
        auto screen = ScreenInteractive::Fullscreen();

        job_list_ = get_job_list();
        if (!job_list_.empty()) {
            selected_index_ = 0;
            update_selected_job();
            reset_scroll_to_bottom();
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
                bool is_finished = false;
                if (it != job_states_.end()) {
                    has_update = it->second.stdout_changed || it->second.stderr_changed;
                    is_finished = it->second.finished;
                }

                Element job_elem = text(job);

                if (i == selected_index_) {
                    job_elem = job_elem | inverted;
                } else if (is_finished) {
                    job_elem = job_elem | bgcolor(Color::Red);
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

            auto stdout_lines = wrap_lines(split_lines(current_stdout_), wrap_width_);
            auto stderr_lines = wrap_lines(split_lines(current_stderr_), wrap_width_);

            stdout_focus_line_ = std::min(stdout_focus_line_, 
                                          std::max(0, static_cast<int>(stdout_lines.size()) - 1));
            stderr_focus_line_ = std::min(stderr_focus_line_,
                                          std::max(0, static_cast<int>(stderr_lines.size()) - 1));

            Element stdout_content;
            if (stdout_lines.empty()) {
                stdout_content = text("(No stdout)") | dim;
            } else {
                Elements stdout_elements;
                stdout_elements.push_back(text("=== STDOUT ===") | bold);
                for (size_t i = 0; i < stdout_lines.size(); ++i) {
                    Element line = text(stdout_lines[i]);
                    if (static_cast<int>(i) == stdout_focus_line_) {
                        line |= focus;
                    }
                    stdout_elements.push_back(line);
                }
                stdout_content = vbox(std::move(stdout_elements)) | vscroll_indicator | yframe;
            }

            Element stderr_content;
            if (stderr_lines.empty()) {
                stderr_content = text("(No stderr)") | dim;
            } else {
                Elements stderr_elements;
                stderr_elements.push_back(text("=== STDERR ===") | bold);
                for (size_t i = 0; i < stderr_lines.size(); ++i) {
                    Element line = text(stderr_lines[i]);
                    if (static_cast<int>(i) == stderr_focus_line_) {
                        line |= focus;
                    }
                    stderr_elements.push_back(line);
                }
                stderr_content = vbox(std::move(stderr_elements)) | vscroll_indicator | yframe;
            }

            bool is_current_finished = false;
            if (!job_list_.empty() && selected_index_ < job_list_.size()) {
                const auto& current_job = job_list_[selected_index_];
                auto it = job_states_.find(current_job);
                if (it != job_states_.end()) {
                    is_current_finished = it->second.finished;
                }
            }

            auto left_pane = stdout_content | flex | frame | border;
            auto right_pane = stderr_content | flex | frame | border;

            if (is_current_finished) {
                left_pane = left_pane | color(Color::Red);
                right_pane = right_pane | color(Color::Red);
            } else if (stdout_pane_focused_) {
                left_pane = left_pane | color(Color::Cyan);
            } else {
                right_pane = right_pane | color(Color::Cyan);
            }

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
                    if (!job_list_.empty() && selected_index_ < job_list_.size()) {
                        const auto& current_job = job_list_[selected_index_];
                        auto it = job_states_.find(current_job);
                        if (it != job_states_.end() && it->second.finished) {
                            it->second.viewed = true;
                        }
                    }
                    --selected_index_;
                    cleanup_viewed_finished_jobs();
                    clear_change_flags();
                    update_selected_job();
                    reset_scroll_to_bottom();
                }
                return true;
            }
            if (event == Event::ArrowRight || event == Event::Character('l')) {
                if (selected_index_ + 1 < job_list_.size()) {
                    if (!job_list_.empty() && selected_index_ < job_list_.size()) {
                        const auto& current_job = job_list_[selected_index_];
                        auto it = job_states_.find(current_job);
                        if (it != job_states_.end() && it->second.finished) {
                            it->second.viewed = true;
                        }
                    }
                    ++selected_index_;
                    cleanup_viewed_finished_jobs();
                    clear_change_flags();
                    update_selected_job();
                    reset_scroll_to_bottom();
                }
                return true;
            }
            if (event == Event::Tab) {
                stdout_pane_focused_ = !stdout_pane_focused_;
                return true;
            }
            if (event == Event::ArrowUp || event == Event::Character('k')) {
                int& focus_line = stdout_pane_focused_ ? stdout_focus_line_ : stderr_focus_line_;
                if (focus_line > 0) {
                    --focus_line;
                }
                return true;
            }
            if (event == Event::ArrowDown || event == Event::Character('j')) {
                const std::string& content = stdout_pane_focused_ ? current_stdout_ : current_stderr_;
                int& focus_line = stdout_pane_focused_ ? stdout_focus_line_ : stderr_focus_line_;
                auto lines = wrap_lines(split_lines(content), wrap_width_);
                if (focus_line + 1 < static_cast<int>(lines.size())) {
                    ++focus_line;
                }
                return true;
            }
            if (event == Event::PageUp) {
                int& focus_line = stdout_pane_focused_ ? stdout_focus_line_ : stderr_focus_line_;
                focus_line = std::max(0, focus_line - 10);
                return true;
            }
            if (event == Event::PageDown) {
                const std::string& content = stdout_pane_focused_ ? current_stdout_ : current_stderr_;
                int& focus_line = stdout_pane_focused_ ? stdout_focus_line_ : stderr_focus_line_;
                auto lines = wrap_lines(split_lines(content), wrap_width_);
                focus_line = std::min(std::max(0, static_cast<int>(lines.size()) - 1), focus_line + 10);
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

        for (auto& [job, state] : job_states_) {
            if (new_job_set.find(job) == new_job_set.end() && !state.finished) {
                state.finished = true;
            }
        }

        for (const auto& job : new_jobs) {
            if (std::find(job_list_.begin(), job_list_.end(), job) == job_list_.end()) {
                job_list_.push_back(job);
            }
        }

        for (const auto& job : job_list_) {
            auto& state = job_states_[job];
            state.name = job;

            if (!state.initialized) {
                state.info = get_job_info(job);
                state.stdout_file = get_job_stdout_file(job);
                state.stderr_file = get_job_stderr_file(job);
                state.initialized = true;
            }

            if (!state.stdout_file.empty()) {
                auto info = file_watcher_.get_file_info(state.stdout_file);
                if (info) {
                    state.stdout_content = info->content;
                    state.stdout_changed = info->has_changed;
                }
            }

            if (!state.stderr_file.empty()) {
                auto info = file_watcher_.get_file_info(state.stderr_file);
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

        auto it = job_states_.find(job);
        if (it != job_states_.end()) {
            current_info_ = it->second.info;
            current_stdout_ = it->second.stdout_content;
            current_stderr_ = it->second.stderr_content;
        } else {
            current_info_ = get_job_info(job);
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

    void reset_scroll_to_bottom() {
        auto stdout_lines = wrap_lines(split_lines(current_stdout_), wrap_width_);
        auto stderr_lines = wrap_lines(split_lines(current_stderr_), wrap_width_);
        stdout_focus_line_ = stdout_lines.empty() ? 0 : static_cast<int>(stdout_lines.size()) - 1;
        stderr_focus_line_ = stderr_lines.empty() ? 0 : static_cast<int>(stderr_lines.size()) - 1;
    }

    void cleanup_viewed_finished_jobs() {
        std::vector<std::string> to_remove;
        for (const auto& job : job_list_) {
            auto it = job_states_.find(job);
            if (it != job_states_.end() && it->second.finished && it->second.viewed) {
                to_remove.push_back(job);
            }
        }
        
        for (const auto& job : to_remove) {
            job_states_.erase(job);
            job_list_.erase(std::remove(job_list_.begin(), job_list_.end(), job), job_list_.end());
        }
        
        if (selected_index_ >= job_list_.size() && !job_list_.empty()) {
            selected_index_ = job_list_.size() - 1;
        }
    }

    int tick_interval_ms_;
    int wrap_width_;
    size_t selected_index_;
    int stdout_focus_line_;
    int stderr_focus_line_;
    bool stdout_pane_focused_;
    std::atomic<bool> running_;

    std::vector<std::string> job_list_;
    std::unordered_map<std::string, JobState> job_states_;
    std::string current_stdout_;
    std::string current_stderr_;
    std::string current_info_;
    std::string previously_selected_job_;

    FileWatcher file_watcher_;
};

void run_monitor(int tick_interval_ms, int wrap_width) {
    MonitorImpl monitor(tick_interval_ms, wrap_width);
    monitor.run();
}

}