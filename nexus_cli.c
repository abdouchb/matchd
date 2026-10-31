#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_TABS 10
#define MAX_HISTORY 100
#define MAX_BOOKMARKS 50
#define MAX_URL_LEN 256

/* ANSI Color Codes for Terminal UI Styling */
#define COLOR_RESET   "\x1b[0m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_PURPLE  "\x1b[35m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GRAY    "\x1b[90m"
#define COLOR_BG_DARK "\x1b[40m"

#define BORDER COLOR_PURPLE "================================================================================" COLOR_RESET "\n"
#define LINE   COLOR_CYAN   "--------------------------------------------------------------------------------" COLOR_RESET "\n"

typedef struct {
    char url[MAX_URL_LEN];
    char title[100];
} Page;

typedef struct {
    int id;
    Page history[MAX_HISTORY];
    int history_count;
    int history_index;
} Tab;

/* Global State Variables */
Tab tabs[MAX_TABS];
int tab_count = 0;
int active_tab_index = 0;
int next_tab_id = 1;

Page bookmarks[MAX_BOOKMARKS];
int bookmark_count = 0;

void load_url(const char* url);
void handle_command(const char* cmd);
void init_browser();
void render_ui();
void toggle_bookmark();
void clear_screen();

void load_url(const char* url) {
    Tab* current_tab = &tabs[active_tab_index];
    char full_url[MAX_URL_LEN];

    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        if (strchr(url, '.') != NULL && strchr(url, ' ') == NULL) {
            snprintf(full_url, MAX_URL_LEN, "https://%s", url);
        } else {
            snprintf(full_url, MAX_URL_LEN, "https://duckduckgo.com/?q=%s", url);
        }
    } else {
        strncpy(full_url, url, MAX_URL_LEN - 1);
    }
    full_url[MAX_URL_LEN - 1] = '\0';

    if (current_tab->history_index < current_tab->history_count - 1) {
        current_tab->history_count = current_tab->history_index + 1;
    }

    if (current_tab->history_count < MAX_HISTORY) {
        current_tab->history_index++;
        strncpy(current_tab->history[current_tab->history_index].url, full_url, MAX_URL_LEN - 1);
        snprintf(current_tab->history[current_tab->history_index].title, 100, "Web Document: %s", full_url);
        current_tab->history_count++;
    }
}

void toggle_bookmark() {
    Tab* tab = &tabs[active_tab_index];
    if (tab->history_count == 0) return;
    char* current_url = tab->history[tab->history_index].url;

    for (int i = 0; i < bookmark_count; i++) {
        if (strcmp(bookmarks[i].url, current_url) == 0) {
            for (int j = i; j < bookmark_count - 1; j++) {
                bookmarks[j] = bookmarks[j+1];
            }
            bookmark_count--;
            return;
        }
    }

    if (bookmark_count < MAX_BOOKMARKS) {
        strncpy(bookmarks[bookmark_count].url, current_url, MAX_URL_LEN - 1);
        strncpy(bookmarks[bookmark_count].title, "Saved Page", 99);
        bookmark_count++;
    }
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void render_ui() {
    clear_screen();
    Tab* current_tab = &tabs[active_tab_index];
    char* current_url = current_tab->history_count > 0 ? current_tab->history[current_tab->history_index].url : "about:blank";

    printf(BORDER);
    printf(COLOR_PURPLE " 🌐 NEXUS TERMINAL BROWSER \n" COLOR_RESET);

    printf(LINE);
    printf(COLOR_GRAY " TABS: " COLOR_RESET);
    for (int i = 0; i < tab_count; i++) {
        if (i == active_tab_index) {
            printf(COLOR_GREEN "[* Space %d] " COLOR_RESET, tabs[i].id);
        } else {
            printf("[Space %d] ", tabs[i].id);
        }
    }
    printf("\n");

    printf(LINE);
    printf(COLOR_YELLOW " ★ BOOKMARKS: " COLOR_RESET);
    if (bookmark_count == 0) printf("None");
    for (int i = 0; i < bookmark_count; i++) {
        printf("%d.[%s] ", i+1, bookmarks[i].url);
    }
    printf("\n");

    printf(BORDER);
    printf(" 🔗 URL:     " COLOR_GREEN "%s" COLOR_RESET "\n", current_url);
    printf(" ⌨️  ACTIONS: [type URL to surf] | " COLOR_CYAN "b" COLOR_RESET ": Back | " COLOR_CYAN "f" COLOR_RESET ": Forward | " COLOR_CYAN "r" COLOR_RESET ": Reload\n");
    printf("            " COLOR_CYAN "t" COLOR_RESET ": New Space | " COLOR_CYAN "c" COLOR_RESET ": Close Space | " COLOR_CYAN "s <id>" COLOR_RESET ": Switch | " COLOR_YELLOW "star" COLOR_RESET ": Bookmark | " COLOR_CYAN "h" COLOR_RESET ": History | " COLOR_RED "q" COLOR_RESET ": Quit\n");
    printf(BORDER);

    printf("\n");
    if (current_tab->history_count > 0) {
        printf("  " COLOR_BG_DARK "                                                                        " COLOR_RESET "\n");
        printf("  " COLOR_BG_DARK COLOR_GREEN "  [ CONNECTED: %-55s ] " COLOR_RESET "\n", current_url);
        printf("  " COLOR_BG_DARK "                                                                        " COLOR_RESET "\n");
        printf("  " COLOR_BG_DARK "  Welcome to the Nexus simulated terminal page.                         " COLOR_RESET "\n");
        printf("  " COLOR_BG_DARK "  This represents the visual viewport of the website.                   " COLOR_RESET "\n");
        printf("  " COLOR_BG_DARK "                                                                        " COLOR_RESET "\n");
        printf("  " COLOR_BG_DARK COLOR_GRAY "  <html lang=\"en\"> ... Content loaded successfully ... </html>          " COLOR_RESET "\n");
        printf("  " COLOR_BG_DARK "                                                                        " COLOR_RESET "\n");
    } else {
        printf("  New Space - Please enter a URL to start surfing.\n");
    }
    printf("\n");
    printf(BORDER);
}

void handle_command(const char* cmd) {
    Tab* current_tab = &tabs[active_tab_index];

    if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
        printf("Closing Nexus...\n");
        exit(0);
    } else if (strcmp(cmd, "b") == 0) {
        if (current_tab->history_index > 0) current_tab->history_index--;
    } else if (strcmp(cmd, "f") == 0) {
        if (current_tab->history_index < current_tab->history_count - 1) current_tab->history_index++;
    } else if (strcmp(cmd, "r") == 0) {
        // Reload action (Just redraws the UI in terminal)
    } else if (strcmp(cmd, "t") == 0) {
        if (tab_count < MAX_TABS) {
            tabs[tab_count].id = next_tab_id++;
            tabs[tab_count].history_count = 0;
            tabs[tab_count].history_index = -1;
            active_tab_index = tab_count;
            tab_count++;
            load_url("duckduckgo.com");
        }
    } else if (strcmp(cmd, "c") == 0) {
        if (tab_count > 1) {
            for (int i = active_tab_index; i < tab_count - 1; i++) {
                tabs[i] = tabs[i+1];
            }
            tab_count--;
            if (active_tab_index >= tab_count) {
                active_tab_index = tab_count - 1;
            }
        }
    } else if (strncmp(cmd, "s ", 2) == 0) {
        int target_id = atoi(cmd + 2);
        for (int i = 0; i < tab_count; i++) {
            if (tabs[i].id == target_id) {
                active_tab_index = i;
                break;
            }
        }
    } else if (strcmp(cmd, "star") == 0) {
        toggle_bookmark();
    } else if (strcmp(cmd, "h") == 0) {
        printf("\n" COLOR_PURPLE "--- BROWSING HISTORY ---" COLOR_RESET "\n");
        for (int i = current_tab->history_count - 1; i >= 0; i--) {
            if (i == current_tab->history_index) {
                printf(COLOR_GREEN " -> %s\n" COLOR_RESET, current_tab->history[i].url);
            } else {
                printf("    %s\n", current_tab->history[i].url);
            }
        }
        printf(COLOR_GRAY "Press Enter to return..." COLOR_RESET);
        getchar(); 
    } else if (strlen(cmd) > 0) {
        load_url(cmd);
    }
}

void init_browser() {
    tabs[0].id = next_tab_id++;
    tabs[0].history_count = 0;
    tabs[0].history_index = -1;
    tab_count = 1;

    strncpy(bookmarks[0].url, "https://en.wikipedia.org", MAX_URL_LEN);
    strncpy(bookmarks[0].title, "Wiki", 100);
    strncpy(bookmarks[1].url, "https://duckduckgo.com", MAX_URL_LEN);
    strncpy(bookmarks[1].title, "Duck", 100);
    bookmark_count = 2;

    load_url("https://en.wikipedia.org/wiki/Main_Page");
}

int main() {
    init_browser();
    char input[256];

    while (1) {
        render_ui();
        printf(COLOR_CYAN "Nexus Cmd> " COLOR_RESET);
        
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Strip newline characters (both \n and \r for cross-platform support)
            input[strcspn(input, "\r\n")] = 0; 
            handle_command(input);
        }
    }
    return 0;
}
