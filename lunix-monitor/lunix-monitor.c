/*
 * lunix-monitor.c
 * * A simple ncurses-based monitor for the Lunix:TNG driver.
 * Displays data from all sensors in real-time.
 *
 * Compile with: gcc -o lunix-monitor lunix-monitor.c -lncurses
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ncurses.h>

#define MAX_SENSORS 16
#define BUF_SIZE 32

/* * Structure to hold the state of one sensor's connection 
 */
struct sensor_data {
    int fd_batt;
    int fd_temp;
    int fd_light;
    char val_batt[BUF_SIZE];
    char val_temp[BUF_SIZE];
    char val_light[BUF_SIZE];
};

struct sensor_data sensors[MAX_SENSORS];

/* * Helper to open a specific device node safely 
 */
int open_sensor_dev(int sensor_id, const char *type) {
    char path[64];
    snprintf(path, sizeof(path), "/dev/lunix%d-%s", sensor_id, type);
    
    // Open in Non-Blocking mode so the UI doesn't freeze!
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    return fd; // Returns -1 if failed (e.g. device doesn't exist)
}

/* * Attempt to read new data from a file descriptor.
 * Returns 1 if updated, 0 otherwise.
 */
int try_read(int fd, char *buffer) {
    if (fd < 0) return 0;

    char temp_buf[BUF_SIZE];
    int bytes = read(fd, temp_buf, BUF_SIZE - 1);
    
    if (bytes > 0) {
        // We got data! Null terminate it.
        temp_buf[bytes] = '\0';
        // Remove newlines for cleaner UI display
        char *newline = strchr(temp_buf, '\n');
        if (newline) *newline = '\0';
        
        // Update only if changed (optional optimization)
        strncpy(buffer, temp_buf, BUF_SIZE);
        return 1;
    }
    return 0;
}

void init_colors() {
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);  // Headers
        init_pair(2, COLOR_GREEN, COLOR_BLACK); // Good values
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);// Sensor ID
        init_pair(4, COLOR_RED, COLOR_BLACK);   // Errors/Offline
    }
}

int main() {
    int i;
    int ch;

    // 1. Initialize Ncurses
    initscr();              // Start curses mode
    cbreak();               // Line buffering disabled
    noecho();               // Don't echo user input
    nodelay(stdscr, TRUE);  // Non-blocking input (so loop continues)
    curs_set(0);            // Hide cursor
    init_colors();

    // 2. Open all device files
    for (i = 0; i < MAX_SENSORS; i++) {
        // Insert a NULL terminator at index 0 to make it an empty string
        sensors[i].val_batt[0] = '\0'; 
        sensors[i].val_temp[0] = '\0';
        sensors[i].val_light[0] = '\0';

        sensors[i].fd_batt = open_sensor_dev(i, "batt");
        sensors[i].fd_temp = open_sensor_dev(i, "temp");
        sensors[i].fd_light = open_sensor_dev(i, "light");
    }

    // 3. Main Loop
    while (1) {
        // Check for User Input ('q' to quit)
        ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        // Poll all sensors for new data
        for (i = 0; i < MAX_SENSORS; i++) {
            try_read(sensors[i].fd_batt, sensors[i].val_batt);
            try_read(sensors[i].fd_temp, sensors[i].val_temp);
            try_read(sensors[i].fd_light, sensors[i].val_light);
        }

        // Draw the UI
        erase(); // Clear screen
        
        // Title
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 2, "Lunix:TNG Sensor Monitor (Press 'q' to quit)");
        attroff(COLOR_PAIR(1) | A_BOLD);

        // *** YOUR NEW DISCLAIMER ***
        attron(A_DIM); // Make it slightly dimmer so it's not distracting
        mvprintw(1, 2, "[This version uses Polling - It does not support Wait-Wake]");
        attroff(A_DIM);

        // Table Header (Moved down to Row 3)
        attron(A_UNDERLINE);
        mvprintw(3, 2, "%-8s  %-12s  %-12s  %-12s", "ID", "Battery(V)", "Temp(C)", "Light");
        attroff(A_UNDERLINE);

        // Table Rows (Moved start to Row 5)
        for (i = 0; i < MAX_SENSORS; i++) {
            int row = 5 + i;
            
            // Highlight Sensor ID
            attron(COLOR_PAIR(3));
            mvprintw(row, 2, "Sensor %02d", i);
            attroff(COLOR_PAIR(3));

            // Print values
            if (sensors[i].fd_batt < 0 || sensors[i].val_batt[0] == '\0') {
                attron(COLOR_PAIR(4));
                mvprintw(row, 12, "%-12s  %-12s  %-12s", "OFFLINE", "OFFLINE", "OFFLINE");
                attroff(COLOR_PAIR(4));
            } else {
                attron(COLOR_PAIR(2));
                mvprintw(row, 12, "%-12s  %-12s  %-12s", 
                         sensors[i].val_batt, 
                         sensors[i].val_temp, 
                         sensors[i].val_light);
                attroff(COLOR_PAIR(2));
            }
        }
        
        // Footer
        mvprintw(5 + MAX_SENSORS + 1, 2, "Status: Active Polling (O_NONBLOCK)...");

        refresh(); // Render to screen
        
        // TODO: Implement select() / poll()
        // Sleep briefly to reduce CPU usage
        usleep(100000); 
    }

    // 4. Cleanup
    for (i = 0; i < MAX_SENSORS; i++) {
        if (sensors[i].fd_batt >= 0) close(sensors[i].fd_batt);
        if (sensors[i].fd_temp >= 0) close(sensors[i].fd_temp);
        if (sensors[i].fd_light >= 0) close(sensors[i].fd_light);
    }
    endwin(); // End curses mode

    return 0;
}