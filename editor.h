#pragma once

void enable_raw_mode();

void init_editor();
void close_editor();
void editor_open(const char *filename);
void editor_refresh_screen();
void editor_process_keypress();