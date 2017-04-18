#include "gui.h"
#include <string.h>

WINDOW *create_help_win(int height, int width, int starty, int startx);
void destroy_help_win(WINDOW *local_win);

WINDOW *create_help_win(int height, int width, int starty, int startx)
{   
    WINDOW *local_win;
    char buffer [80];

    local_win = newwin(height, width, starty, startx);
    box(local_win, 0 , 0);      /* 0, 0 gives default characters 
                     * for the vertical and horizontal
                     * lines            */
    wbkgd( local_win, COLOR_PAIR( 2 ) );
    color_set( 2, NULL );

    

    sprintf( buffer, " Command set for the LoRa Gateway ");
    mvaddstr( 1, ( 80 - strlen( buffer ) ) / 2, buffer );

    sprintf( buffer, "Channel 0 commands are lower case (a-z)");
    mvaddstr( 3, ( 80 - strlen( buffer ) ) / 2, buffer );

    sprintf( buffer, "Channel 1 commands are upper case (A-Z)");
    mvaddstr( 4, ( 80 - strlen( buffer ) ) / 2, buffer );

    sprintf( buffer, "A = +100MHz    Z= -100MHz");
    mvaddstr( 6, ( 80 - strlen( buffer ) ) / 2, buffer );

    sprintf( buffer, "S = +10MHz     X= -10MHz ");
    mvaddstr( 7, ( 80 - strlen( buffer ) ) / 2, buffer );

    sprintf( buffer, "D = +1MHz      C= -1MHz  ");
    mvaddstr( 8, ( 80 - strlen( buffer ) ) / 2, buffer );

    sprintf( buffer, "F = Toggle AFC ");
    mvaddstr( 10, ( 80 - strlen( buffer ) ) / 2, buffer );

    sprintf( buffer, "M = Toggle Modes (This is not implemented yet!!!) ");
    mvaddstr( 12, ( 80 - strlen( buffer ) ) / 2, buffer );

    wrefresh(local_win);        /* Show that box        */

    return local_win;
}

void destroy_help_win(WINDOW *local_win)
{   
    int i = 0;
    char buffer [80];

    // Remove the border
    wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');

    color_set( 3, NULL );
    for (i=1;i<15;i++)
        mvaddstr(i, 39, "  ");

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

}
