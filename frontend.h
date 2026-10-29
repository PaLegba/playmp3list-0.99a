#ifndef _CLASS_MPG123FRONTEND_
#define _CLASS_MPG123FRONTEND_

#include "playmp3list.h"
#include "interface.h"
#include <sys/timeb.h>

class mpg123frontend
{ private:
    playstate_type playstate;
    playmp3listWindow *interface;
    int mpg123_pid, mpg123_send, mpg123_recv;
    int initialised, connected;
    int readline();
    char *trimleft(char *);
    char *trimright(char *);
    char *trim(char *);
    int buflen, last_frameremain, last_totaltime;
#define paramlen 256
    char *param_list[paramlen];
#define BUF_SIZE 512
    char buf[BUF_SIZE];
    int num_inits;
    int priority;
    struct timeb seeklast;
    int seekvalue;
    bool stopping;
    char id3v2_title[256];
    char id3v2_artist[256];
    char id3v2_album[256];
    bool have_id3v2_tags;
    void clear_id3v2();
    void build_id3v2_name();
    
  public:
    mpg123frontend(playmp3listWindow *, char*, int);
    ~mpg123frontend();
    
    void killmpg123();
    bool init_mpg123();
    bool process_messages(bool &);
    void open_file(char *);
    playstate_type get_state();
    void action_play();
    void action_stop();
    void action_pause();
    void action_ffwd();
    void action_rwnd();
    void set_volume(int pct);
};

#endif // _CLASS_MPG123FRONTEND_
