#ifndef _CLASS_PMP3LWINDOW_
#define _CLASS_PMP3LWINDOW_

#include "playmp3list.h"
#include NCURSES
#include "playlist.h"

class playmp3listWindow
{ private:
    int lines, cols;
    int elapsedtime, remainingtime, totaltime;
    int freq, bitrate, volume;
    bool stereo, shuffle, repeat;
    playstate_type playstate;
    mp3playlist *playlist;
    int pos, top, playpos;
    bool timeremaining_mode;
    int pcolor[12][3];
    chtype style[12];
    char *filename, *listname;
    
  public:
    bool fullpath;

    playmp3listWindow(char *, mp3playlist *, int[12][3]);
    ~playmp3listWindow();

    void draw_statics();
    void draw_progress();
    void show_noinfo();
    void set_filename(const char *);
    void draw_filename();
    void set_listname(char *);
    void draw_listname();
    void set_format(int, int, bool);
    void draw_format();
    void set_shuffle(bool);
    void draw_shuffle();
    void set_repeat(bool);
    void draw_repeat();
    void set_volume(int);
    void draw_volume();
    void set_playstate(playstate_type);
    playstate_type get_playstate();
    void draw_playstate();
    void set_elapsedtime(int, int);
    void draw_elapsedtime();
    void set_totaltime(int);
    void draw_totaltime();
    void scrollto(int);
    void set_playpos(int);
    void set_pos(int);
    void set_timeremaining_mode(bool);
    void draw_list();
    void erase_list();
    void draw_help();
    void draw_info(char **);
    void action_next(chtype);
    void action_up();
    void action_down();
    void action_home();
    void action_end();
    void action_pgup();
    void action_pgdn();
    int  get_listpos();
    int  get_ch();
    void resize();
    void do_refresh();
    void set_colors(int[12][3]);
};

#endif // _CLASS_PMP3LWINDOW_
