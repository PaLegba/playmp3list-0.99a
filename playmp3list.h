#ifndef _PLAYMP3LIST_H_
#define _PLAYMP3LIST_H_

#define NCURSES <ncurses.h>

enum playstate_type { NOPLAY, STOPPED, PLAYING, PAUSED };

enum file_type_type { FT_DIR, FT_PLAYLIST, FT_SONG, FT_OTHER };

/* Maximum possible length of the playlist - is this enough? */
#define PL_MAX 3000

/* Comment this out for mono screen theme by default */
#define COLOR_SCREEN

#define act_nothing                 0
#define act_previous_song           1
#define act_play_song               2
#define act_pause_song              3
#define act_stop_song               4
#define act_next_song               5
#define act_rewind_song             6
#define act_fast_forward_song       7
#define act_up                      8
#define act_down                    9
#define act_home                    10
#define act_end                     11
#define act_page_up                 12
#define act_page_down               13
#define act_play_selected           14
#define act_increase_volume         15
#define act_decrease_volume         16
#define act_toggle_shuffle          17
#define act_toggle_repeat           18
#define act_toggle_full_path        19
#define act_toggle_time_remaining   20
#define act_toggle_alpha            21
#define act_jump_root               22
#define act_jump_home               23
#define act_help                    24
#define act_redraw                  25
#define act_quit                    26
#define act_resize                  27
#define act_jump_updir		    28
#define act_jump_user1              29
#define act_jump_user2              30
#define act_jump_user3              31
#define act_jump_user4              32
#define act_jump_user5              33
#define act_jump_user6              34
#define act_jump_user7              35
#define act_jump_user8              36
#define act_jump_user9              37
#define act_jump_user0              38
#define act_show_ID3_tag	    39
#define act_search		    40

#endif // _PLAYMP3LIST_H_
