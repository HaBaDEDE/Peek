#pragma once

#include <string>

namespace peek {

enum class Language {
    English,
    ChineseSimplified,
};

enum class CloseBehavior {
    Ask = 0,
    MinimizeToTray = 1,
    Exit = 2,
};

struct Strings {
    const wchar_t* ghost_tab;
    const wchar_t* unlock_tab;
    const wchar_t* focus_tab;
    const wchar_t* language_switch;
    const wchar_t* ghost_waiting;
    const wchar_t* ghost_question;
    const wchar_t* unlock_question;
    const wchar_t* focus_question;
    const wchar_t* unknown_process;
    const wchar_t* from;
    const wchar_t* open_location;
    const wchar_t* enter_focus;
    const wchar_t* drop_file;
    const wchar_t* choose_file;
    const wchar_t* no_owner;
    const wchar_t* owners_found;
    const wchar_t* end_process;
    const wchar_t* inspecting;
    const wchar_t* start_inspect;
    const wchar_t* move_pointer;
    const wchar_t* process;
    const wchar_t* exe;
    const wchar_t* hwnd;
    const wchar_t* window_class;
    const wchar_t* title;
    const wchar_t* rectangle;
    const wchar_t* control_type;
    const wchar_t* name;
    const wchar_t* automation_id;
    const wchar_t* value;
    const wchar_t* uia_bounds;
    const wchar_t* unavailable;
    const wchar_t* open_process_location;
    const wchar_t* started;
    const wchar_t* exited;
    const wchar_t* window_created;
    const wchar_t* shown;
    const wchar_t* hidden;
    const wchar_t* destroyed;
    const wchar_t* tray_open;
    const wchar_t* tray_language;
    const wchar_t* tray_exit;
    const wchar_t* picker_title;
    const wchar_t* all_files;
    const wchar_t* terminate_question;
    const wchar_t* unsaved_warning;
    const wchar_t* terminate_title;
    const wchar_t* main_class_error;
    const wchar_t* main_window_error;
    const wchar_t* ghost_hotkey_error;
    const wchar_t* tray_settings;
    const wchar_t* close_question;
    const wchar_t* close_explanation;
    const wchar_t* minimize_to_tray;
    const wchar_t* exit_peek;
    const wchar_t* remember_choice;
    const wchar_t* settings_title;
    const wchar_t* settings_instruction;
    const wchar_t* settings_explanation;
    const wchar_t* ask_every_time;
};

[[nodiscard]] const Strings& GetStrings(Language language);
[[nodiscard]] Language LoadLanguage();
void SaveLanguage(Language language);
[[nodiscard]] CloseBehavior LoadCloseBehavior();
void SaveCloseBehavior(CloseBehavior behavior);

} // namespace peek
