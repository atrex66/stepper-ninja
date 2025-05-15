#ifndef SERIAL_TERMINAL_H
#define SERIAL_TERMINAL_H

void save_configuration();
void load_configuration();
void handle_serial_input();
void process_command(char* command); // Csak char* command, mert net_info globális

#endif
// SERIAL_TERMINAL_H 