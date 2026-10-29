#ifndef _CLASS_MP3PLAYLIST_
#define _CLASS_MP3PLAYLIST_

#include "playmp3list.h"

class mp3playlist
/* Class for managing all entries associated with a playlist */
{ private:
    char *list[PL_MAX];
    char *shortlist[PL_MAX];
    file_type_type typelist[PL_MAX];
    int index[PL_MAX], n, m, s;
//    char listname[256], currdir[512];
    char listname[1024], currdir[4096];
    
  public:
    mp3playlist();
    ~mp3playlist();
    void  clear_list();
    bool  add_item(char *, char*, file_type_type);
    bool  add_item(char *, bool);
    bool  is_a_dir(char *);
    int   ch_dir(char *);
    bool  load(char *, bool, bool, bool);
    bool  load_dir(char *, bool, bool, bool);
    bool  load_playlist(char *, bool, bool, bool);
    bool  reload(bool, bool, bool);
    char* item(int);
    char* shortitem(int);
    file_type_type file_type(int);
    file_type_type guess_file_type(char *);
    bool  checklist(char *);
    int   count();
    int   song_count();
    void  shuffle();
    void  unshuffle();
    void  sort(bool, bool);
    bool  next(bool);
    bool  prev();
    int   curr();
    void  set_s(int);
    int   get_s();
    char* get_listname();
}; 

#endif // _CLASS_MP3PLAYLIST_
