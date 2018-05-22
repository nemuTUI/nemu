#include <ncurses.h> 
#include <unistd.h> 
#include <string.h>

int main(void) 
{
    int parent_x, parent_y; 
    int score_size; 
    initscr(); 
    noecho(); 
    curs_set(FALSE); 
    start_color();
    use_default_colors();
    init_pair(1,COLOR_BLACK, COLOR_WHITE);
    // get our maximum window dimensions 
    getmaxyx(stdscr, parent_y, parent_x); 
    score_size = parent_x * 0.7;
    // set up initial windows 
    WINDOW *help = newwin(1, parent_x, 0, 0);
    WINDOW *field = newwin(parent_y - 1, parent_x - score_size, 1, 0);
    WINDOW *score = newwin(parent_y - 1, score_size, 1, parent_x - score_size);
    wbkgd(help, COLOR_PAIR(1));
    box(field, 0, 0);
    box(score, 0, 0);
    // draw to our windows 
    mvwprintw(help, 0, 0, " help [F1]"); 
    mvwprintw(field, 1, ((parent_x - score_size) - strlen("VM list"))/2, "VM list"); 
    mvwaddch(field, 2, 0, ACS_LTEE);
    mvwhline(field, 2, 1, ACS_HLINE, parent_x - score_size - 2);
    mvwaddch(field, 2, parent_x - score_size - 1, ACS_RTEE);
    mvwprintw(score, 1, (score_size - strlen("Action"))/2, "Action");
    mvwaddch(score, 2, 0, ACS_LTEE);
    mvwhline(score, 2, 1, ACS_HLINE, score_size - 2);
    mvwaddch(score, 2, score_size - 1, ACS_RTEE);
    // refresh each window 
    wrefresh(help); 
    wrefresh(field); 
    wrefresh(score);
    getchar();
    // clean up 
    delwin(help); 
    delwin(field); 
    delwin(score);
    endwin(); 
    return 0;
}
