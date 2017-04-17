#include "gui.h"

extern int help_win_displayed;

WINDOW *create_help_win(int height, int width, int starty, int startx);
void destroy_help_win(WINDOW *local_win);

WINDOW *create_help_win(int height, int width, int starty, int startx)
{   WINDOW *local_win;

    local_win = newwin(height, width, starty, startx);
    box(local_win, 0 , 0);      /* 0, 0 gives default characters 
                     * for the vertical and horizontal
                     * lines            */
    wbkgd( local_win, COLOR_PAIR( 2 ) );
    wrefresh(local_win);        /* Show that box        */

    return local_win;
}

void destroy_help_win(WINDOW *local_win)
{   
    /* box(local_win, ' ', ' '); : This won't produce the desired
     * result of erasing the window. It will leave it's four corners 
     * and so an ugly remnant of window. 
     */
    wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
    /* The parameters taken are 
     * 1. win: the window on which to operate
     * 2. ls: character to be used for the left side of the window 
     * 3. rs: character to be used for the right side of the window 
     * 4. ts: character to be used for the top side of the window 
     * 5. bs: character to be used for the bottom side of the window 
     * 6. tl: character to be used for the top left corner of the window 
     * 7. tr: character to be used for the top right corner of the window 
     * 8. bl: character to be used for the bottom left corner of the window 
     * 9. br: character to be used for the bottom right corner of the window
     */
    wrefresh(local_win);
    delwin(local_win);
}

void gui_show_help ()
{

    WINDOW *help_win;

    int startx, starty, width, height;
    int ch;

    height = 14;
    width = 76;
    starty = 1;  /* Calculating for a center placement */
    startx = 2;    /* of the window        */
    help_win = create_help_win(height, width, starty, startx);

    while((ch = getch()) != 'c')
    {   
        switch(ch)
        {   case KEY_LEFT:
                destroy_help_win(help_win);
                help_win = create_help_win(height, width, starty,--startx);
                break;
            case KEY_RIGHT:
                destroy_help_win(help_win);
                help_win = create_help_win(height, width, starty,++startx);
                break;
            case KEY_UP:
                destroy_help_win(help_win);
                help_win = create_help_win(height, width, --starty,startx);
                break;
            case KEY_DOWN:
                destroy_help_win(help_win);
                help_win = create_help_win(height, width, ++starty,startx);
                break;  
        }
    }

    help_win_displayed = 0;

}
