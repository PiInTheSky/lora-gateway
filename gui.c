#include "gui.h"
#include <string.h>

// extern int help_win_displayed;

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
    int i = 0;
    char buffer [80];

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


    for (i=1;i<15;i++)
        mvchgat(i, 39, 2, COLOR_PAIR(3), 0, NULL);


    // Put the HELP message back
    sprintf( buffer, "             Press (H) for Help             ");
    color_set( 3, NULL );
    mvaddstr( 15, ( 80 - strlen( buffer ) ) / 2, buffer );

    wrefresh(local_win);
    delwin(local_win);
}

void gui_show_help ()
{
    char ch;
    char buffer [80];
    int startx, starty, width, height;

    WINDOW *help_win;

    height = 14;
    width = 76;
    starty = 1;  /* Calculating for a center placement */
    startx = 2;    /* of the window        */
    help_win = create_help_win(height, width, starty, startx);
    sprintf( buffer, "Press any key to return to the main screen!");
    color_set( 3, NULL );
    mvaddstr( 15, ( 80 - strlen( buffer ) ) / 2, buffer );

    // Wait for any key to be pressed
    while ((ch=getch()) == 255)
    {
    };

    destroy_help_win(help_win);
    // help_win_displayed = 0;

}
