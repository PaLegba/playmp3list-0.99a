/* playmp3list - An ncurses-based MP3 player for Linux
 * Copyright (C) Paul Urban (urban@rucus.ru.ac.za)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
      
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
	  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 * This is the main part of `playmp3list'
 * It initialises, then receives keystrokes from the interface and
 * updates the interface and player objects accordingly.
 */

#include "playmp3list.h"
#include "interface.h"
#include "frontend.h"  // in place of "mp3player.h" from older versions
#include "playlist.h"
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <locale.h>

#include "genre.h"

volatile sig_atomic_t resize_pending = 0;

void
on_sigwinch(int sig)
{ (void)sig;
  resize_pending = 1;
}

bool shuffle;
int volume = -1;
playmp3listWindow *interface;
mpg123frontend *player;
mp3playlist *playlist;
int color[12][3];    // Interface element colors
short keymap[128][2]; // space for 128 key-mappings
short nkeys = 0;
int action = 0;
int ch, k;
bool found = false, endofsong = false;
char mpg123params[256]; bool mpg123params_set = false;
char currsongname[1024], prevdirname[1024], fullname_buf[1024];
char jump_dir1[256],jump_dir2[256],jump_dir3[256],jump_dir4[256],jump_dir5[256];
char jump_dir6[256],jump_dir7[256],jump_dir8[256],jump_dir9[256],jump_dir0[256];
char theme_name[256]; bool theme_name_set = false;

#define SETCOLOR(n,fg,bg,att) { \
color[n][0] = fg; color[n][1] = bg; color[n][2] = att; \
}

#define MAPKEY(key,func) { \
keymap[nkeys][0] = toupper(key); keymap[nkeys][1] = func; \
if (nkeys < 128) nkeys++; \
}

//------------------ some default options - set on the command line
bool autoplay = false, autoshuffle = false, repeat = false,
     autoalpha = false, fullpath = false, time_remaining = false,
     ID3v1 = false, save_debug = false;
int  initial_volume = -1, priority = 0;
//------------------ end options
bool autoplay_set = false, autoshuffle_set = false, repeat_set = false,
     autoalpha_set = false, fullpath_set = false, time_remaining_set = false,
     ID3v1_set = false, save_debug_set = false,
     initial_volume_set = false, priority_set = false;

void panic(int sig)
{ signal(sig, SIG_DFL);
  fprintf(stderr, "Caught signal: %d\n\n",sig);
  delete player;
  delete interface;
  delete playlist;
  switch (sig)
    { case SIGSEGV : printf("Caught signal SIGSEGV (Segmentaion fault)\n\nPlease re-create this error, with debug logging enabled - i.e. run with\ncommand-line option -d, then e-mail ~/playmp3list.debug together with a\ndescription of what happened to the author, urban@rucus.ru.ac.za\n\n"); break;
      case SIGINT : printf("Caught signal: SIGINT\n\n"); break;
      case SIGTERM : printf("Caught signal: SIGTERM\n\n"); break;
      default : printf("Caught signal: %d\n\n",sig); break;
    };
  exit(0);
} // panic

char *fullnameof(char *shortname)
{ if (shortname[0] == '/' || !strncmp(shortname,"http://",7))
   { return shortname;
   }
  else // Add current directory to front of filename
   { getcwd(fullname_buf, 1023);
      strcat(fullname_buf, "/");
      strcat(fullname_buf, shortname);
      return fullname_buf;
   }
}

void startplaying(int s)
/* Starts playing a given song */
{ if (s < 0) return;
  interface->set_playpos(s);
  interface->show_noinfo();
  interface->set_playstate(NOPLAY);
  interface->set_filename("");
  strcpy(currsongname,fullnameof(playlist->item(s)));
  player->open_file(currsongname);
  
  /*strcpy(currsongname, fullname);
  if (playlist->item(s)[0] == '/' || !strncmp(playlist->item(s),"http://",7))
   { player->open_file(playlist->item(s));
     strcpy(currsongname, playlist->item(s));
   }
  else // Add current directory to front of filename
   { char fullname[1024];
     getcwd(fullname, 1023);
     strcat(fullname, "/");
     strcat(fullname, playlist->item(s));
     player->open_file(fullname);
     strcpy(currsongname, fullname);
   }*/
} // startplaying

void set_volume(int pvolume)
/* Set volume 0-100 via mpg123 SCALE command */
{ if (pvolume < 0) pvolume = 0;
  if (pvolume > 100) pvolume = 100;
  volume = pvolume;
  if (player) player->set_volume(pvolume);
  if (interface) interface->set_volume(pvolume);
} // set_volume

int up_volume(int pcnt)
/* Increase volume by pcnt% */
{ int newvol = volume + pcnt;
  if (newvol > 100) newvol = 100;
  set_volume(newvol);
  return newvol;
} // up_volume

int down_volume(int pcnt)
/* Decrease volume by pcnt% */
{ int newvol = volume - pcnt;
  if (newvol < 0) newvol = 0;
  set_volume(newvol);
  return newvol;
} // down_volume

int posinlist(char *searchname)
/* See if the song 'searchname' exists in the list */
{ char *thisfullname;
  int c = 0;
  bool found = false;
  while (c <= playlist->count() && !found) 
   { thisfullname = fullnameof(playlist->item(c));
     if (!strcmp(thisfullname,searchname))
       found = true; else c++;
   }
  if (found) return c; else return -1;
} // posinlist

void prepare_new_list()
{ int c = posinlist(currsongname);
  if (c != -1) playlist->set_s(c);
  interface->set_playpos(c);
  if (shuffle) playlist->shuffle();
  interface->set_listname(playlist->get_listname());
  c = posinlist(prevdirname);
  getcwd(prevdirname, 1023);
  interface->erase_list();
  if (c != -1) interface->set_pos(c);
  else interface->action_home(); // go to top & redraw
  interface->do_refresh(); // patch for Gnome term
} // load_resource

void display_ID3v1_info(char *filename)
/* Display ID3v1 tag info of a given file */
{ if (playlist->guess_file_type(filename) == FT_SONG)
   { FILE *file = fopen(filename, "r");
     if (!file) return;
     fseek(file,-128,SEEK_END);
     char buffer[128];
     fread(buffer,1,128,file);
     fclose(file);
     if (!strncmp(buffer, "TAG", 3))
      { char title[31],artist[31],album[31],comment[31];

#define TRIM(dest) {        \
  dest[30] = '\0';          \
  for (int i=29;i>=0;i--)   \
  { if (dest[i]==' ')       \
      dest[i]='\0';         \
    else                    \
      break;                \
  }                         \
}
			       
#define EXTRACT(dest,src) { \
  strncpy(dest, src, 30);   \
  TRIM(dest);		    \
}
        EXTRACT(title,buffer+3);
	EXTRACT(artist, buffer+33);
	EXTRACT(album, buffer+63);
	EXTRACT(comment, buffer+97);
        char genre[31];
	unsigned int genre_code = buffer[127];
	if (genre_code <= 147)
	 { strncpy(genre, genre_table[genre_code], 30); 
	   TRIM(genre);
	 }
	else strcpy(genre, "Unknown");
	char *temp[9];
	int lc;
	for (lc = 0; lc < 9; lc++) temp[lc] = new char[256];
	sprintf(temp[0],"ID3v1 TAG INFO");
	sprintf(temp[1],"--------------");
	sprintf(temp[2],"Filename: %s",filename);
	sprintf(temp[3],"Title: %s",title);
	sprintf(temp[4],"Artist: %s",artist);
	sprintf(temp[5],"Album: %s",album);
	sprintf(temp[6],"Genre: %s",genre);
	sprintf(temp[7],"Comment: %s",comment);
	strcpy(temp[8],"");
	interface->draw_info(temp);
	for (lc = 0; lc < 9; lc++) delete[] temp[lc];
      }
    }
} // display_ID3v1_info

void play_list(void)
/* The main loop - gets events and responds to them, or keeps playing song */
{ bool playing = true;
  int s = -1;
  interface->set_playstate(NOPLAY);
  interface->set_timeremaining_mode(time_remaining);
  interface->show_noinfo();
  interface->fullpath = fullpath;
  interface->set_playpos(-1);
  if (initial_volume >= 0 && initial_volume <= 100) set_volume(initial_volume);
  else set_volume(100);

  if (autoshuffle) { interface->set_shuffle(shuffle = true);
                     playlist->shuffle(); }
  interface->set_repeat(repeat);

  interface->draw_list();

  if (autoplay)
    { playlist->set_s(0);
      startplaying(playlist->curr());
    }
  
  do
    { playing = player->process_messages(endofsong);
      if (!playing)  // mpg123 process ended
       { fprintf(stderr, "Main loop: mpg123 child process died!\n");
         if (player->init_mpg123()) // try to re-start it...
          { playing = true;
	    interface->show_noinfo();
 	    //endofsong = true;
	  }
        }

      if (endofsong)
       { endofsong = false;
	 action = act_next_song;
         fprintf(stderr, "End of song reached.\n");
       }
      else
       { if (resize_pending)
           { resize_pending = 0;
             interface->resize();
             ch = ERR;
           }
         else
           ch = toupper(interface->get_ch());

         if (ch == KEY_RESIZE)
           { interface->resize(); ch = ERR; }
         action = act_nothing;
         k = 0; found = false;
         do     // See which function the key is mapped to (linear search)
          { if (keymap[k][0] == ch) { action = keymap[k][1]; found = true; }
            k++;
          } while (!found && k < nkeys);
       }
       if (action == act_nothing);
       else if (found) fprintf(stderr,"Got key (%d) which maps to action %d\n",ch,action);
       else fprintf(stderr,"Got key (%d) with no mapping\n",ch);

      switch (action)
        { case               act_nothing : break;
	  case         act_previous_song : if (!playlist->prev()) break;
                                           startplaying(s = playlist->curr());
                                           break;
          case             act_play_song : if (interface->get_playstate() == PAUSED)
					    player->action_pause();
					   else if (interface->get_playstate() == STOPPED || interface->get_playstate() == PLAYING || interface->get_playstate() == NOPLAY)
					     player->open_file(currsongname);
                                           break;
          case            act_pause_song : player->action_pause();
                                           break;
          case             act_stop_song : player->action_stop();
                                           break;
          case             act_next_song : if (!playlist->next(repeat)) { player->killmpg123(); break; };
                                           startplaying(s = playlist->curr());
                                           break;
	  case           act_rewind_song : player->action_rwnd();
					   break;
	  case     act_fast_forward_song : player->action_ffwd();
					   break;
          case                    act_up : interface->action_up();
					   break;
          case                  act_down : interface->action_down();
					   break;
          case                  act_home : interface->action_home(); 
					   break;
          case                   act_end : interface->action_end(); 
					   break;
          case               act_page_up : interface->action_pgup(); 
					   break;
          case             act_page_down : interface->action_pgdn(); 
					   break;
          case         act_play_selected : s = interface->get_listpos();
	                                   if (playlist->file_type(s) == FT_SONG)
				            { if (!shuffle) 
				               playlist->set_s(s);
				              else
					      if (playlist->get_s() == -1)
                                                playlist->set_s(0);
                                              startplaying(s);
				            }
				           else
				            { if (playlist->load(playlist->item(s),ID3v1,autoalpha,fullpath))
					        prepare_new_list();
				            }
                                           break;
	  case       act_increase_volume : up_volume(1); break;
	  case       act_decrease_volume : down_volume(1); break;
          case        act_toggle_shuffle : interface->set_shuffle(shuffle = !shuffle);
                                           if (shuffle) playlist->shuffle();
                                           else  
				            { playlist->unshuffle();
				              playlist->set_s(s); }
                                           break;
          case         act_toggle_repeat : interface->set_repeat(repeat = !repeat);
                                           break;
          case      act_toggle_full_path : fullpath = interface->fullpath = !interface->fullpath;
                                           interface->draw_list();
                                           break;
	  case act_toggle_time_remaining : interface->set_timeremaining_mode(time_remaining = !time_remaining);
	                        	   break;
          case          act_toggle_alpha : autoalpha = !autoalpha;
					   if (playlist->reload(ID3v1,autoalpha,fullpath))
					    { playlist->sort(autoalpha, fullpath);
					      prepare_new_list();
					    }
                                           break;
	  case             act_jump_root : if (playlist->load_dir("/", ID3v1, autoalpha,fullpath))
					     prepare_new_list();
					   break;
	  case             act_jump_home : if (playlist->load_dir(getenv("HOME"),ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break;
	  case                  act_help : interface->draw_help();
				           break;
	  case                act_redraw : interface->do_refresh();
	                                   break;
          case                  act_quit : playing = false;
                                           break;
          case                act_resize : interface->resize();
				           break;
	  case	    	  act_jump_updir : if (playlist->load_dir(playlist->item(0),ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break;
	  case	  	  act_jump_user1 : if (playlist->load(jump_dir1,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user2 : if (playlist->load(jump_dir2,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user3 : if (playlist->load(jump_dir3,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user4 : if (playlist->load(jump_dir4,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user5 : if (playlist->load(jump_dir5,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user6 : if (playlist->load(jump_dir6,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user7 : if (playlist->load(jump_dir7,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user8 : if (playlist->load(jump_dir8,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user9 : if (playlist->load(jump_dir9,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case	  	  act_jump_user0 : if (playlist->load(jump_dir0,ID3v1,autoalpha,fullpath))
	                                     prepare_new_list();
					   break; 
	  case		act_show_ID3_tag : display_ID3v1_info(playlist->item(interface->get_listpos()));
	                                   break;
          case                act_search : while ((ch=interface->get_ch()) == ERR);
	                                   interface->action_next(ch);
					   break;
        };
  } while (playing);
}; // play_list

// This new rc code below is a huge mess!!!
bool load_rcfile()
{ bool founderror = false, foundtheme = false;
  char *rcfilename = new char[100];
  strcpy(rcfilename,getenv("HOME"));
  strcat(rcfilename,"/.playmp3listrc");
#define none		0
#define key_mappings	1
#define settings	2
#define theme		3
  int section = none;
  int linenum = 0;
  FILE *file = fopen(rcfilename, "r");
  delete[] rcfilename;
  if (file) printf("Reading rc file: ~/.playmp3listrc...\n");
  else 
  if((file = fopen("/etc/playmp3listrc", "r")))
    printf("Reading rc file: /etc/playmp3listrc...\n");
  else return false;
  char line[256];
  //strcpy(theme_name,"default");
  char* thetoken;
  while (fgets(line, 255, file))
   { linenum++;
     if (strlen(line) <= 1) continue;
     if (line[0] == '#');     // Comment - ignore
     else if (line[0] == '.') // Section header
      { section = none;
        thetoken = strtok((char*)line+1, " \t\n\r");
        if (!strcasecmp(thetoken,"key_mappings")) section = key_mappings;
        else if (!strcasecmp(thetoken,"settings")) section = settings;
        else if (!strcasecmp(thetoken,"theme"))
         { thetoken = strtok(NULL,"\n");
	   if (!strcasecmp(thetoken,theme_name)) 
            { section = theme;
	      printf("Loading theme: '%s'\n",theme_name);
	      foundtheme = true;
            }
         }
        else { printf("* Unrecognised section header on line %d: %s\n",linenum,thetoken);
               founderror = true; }
      }
     else		 // A command?
      { thetoken = strtok((char*)line, " \t\n\r");
        int action_code = -1;
        int element_code = -1;

#define CHECK_KEY_MAPPING(str,num) \
if (!strcasecmp(thetoken,str)) action_code = num; else

#define CHECK_THEME_ELEMENT(str,num) \
if (!strcasecmp(thetoken,str)) element_code = num; else

#define CHECK_COLOR_CODE(str,num) \
if (!strcasecmp(thetoken,str)) color_code = num; else

#define CHECK_BOOLEAN_SETTING(str,setting, set_flag) \
if (!strcasecmp(thetoken,str)) \
 { if (!set_flag && (thetoken = strtok(NULL," \t\r\n")))  \
    { if (!strcasecmp(thetoken,"true")) setting = true; \
      else if (!strcasecmp(thetoken,"false")) setting = false; \
      else \
       { printf("* Bad boolean value on line %d: %s\n",linenum,thetoken); \
         founderror = true; \
       } \
    } \
 } else

#define CHECK_JUMP_DIR(jump_dir, jump_dir_str) if (!strcasecmp(thetoken, jump_dir)) \
 { char *foo = strtok(NULL,"\t\r\n"); \
   if (foo) strcpy(jump_dir_str, foo); \
 } \
else

/* Note : Take be very careful to have the right number of ;'s here! */

	switch(section)
         { case key_mappings  :
             CHECK_KEY_MAPPING("previous_song",act_previous_song)
	     CHECK_KEY_MAPPING("play_song",act_play_song)
             CHECK_KEY_MAPPING("pause_song",act_pause_song)
             CHECK_KEY_MAPPING("stop_song",act_stop_song)
             CHECK_KEY_MAPPING("next_song",act_next_song)
             CHECK_KEY_MAPPING("rewind_song",act_rewind_song)
             CHECK_KEY_MAPPING("fast_forward_song",act_fast_forward_song)
             CHECK_KEY_MAPPING("up",act_up)
             CHECK_KEY_MAPPING("down",act_down)
             CHECK_KEY_MAPPING("home",act_home)
             CHECK_KEY_MAPPING("end",act_end)
             CHECK_KEY_MAPPING("page_up",act_page_up)
             CHECK_KEY_MAPPING("page_down",act_page_down)
             CHECK_KEY_MAPPING("play_selected",act_play_selected)
             CHECK_KEY_MAPPING("increase_volume",act_increase_volume)
             CHECK_KEY_MAPPING("decrease_volume",act_decrease_volume)
             CHECK_KEY_MAPPING("toggle_shuffle",act_toggle_shuffle)
             CHECK_KEY_MAPPING("toggle_repeat",act_toggle_repeat)
             CHECK_KEY_MAPPING("toggle_full_path",act_toggle_full_path)
             CHECK_KEY_MAPPING("toggle_time_remaining",act_toggle_time_remaining)
             CHECK_KEY_MAPPING("toggle_alpha",act_toggle_alpha)
             CHECK_KEY_MAPPING("jump_root",act_jump_root)
             CHECK_KEY_MAPPING("jump_home",act_jump_home)
             CHECK_KEY_MAPPING("help",act_help)
             CHECK_KEY_MAPPING("redraw",act_redraw)
             CHECK_KEY_MAPPING("quit",act_quit)
             CHECK_KEY_MAPPING("resize",act_resize)
	     CHECK_KEY_MAPPING("jump_updir",act_jump_updir)
	     CHECK_KEY_MAPPING("jump_user1",act_jump_user1)
	     CHECK_KEY_MAPPING("jump_user2",act_jump_user2)
	     CHECK_KEY_MAPPING("jump_user3",act_jump_user3)
	     CHECK_KEY_MAPPING("jump_user4",act_jump_user4)
	     CHECK_KEY_MAPPING("jump_user5",act_jump_user5)
	     CHECK_KEY_MAPPING("jump_user6",act_jump_user6)
	     CHECK_KEY_MAPPING("jump_user7",act_jump_user7)
	     CHECK_KEY_MAPPING("jump_user8",act_jump_user8)
	     CHECK_KEY_MAPPING("jump_user9",act_jump_user9)
	     CHECK_KEY_MAPPING("jump_user0",act_jump_user0)
	     CHECK_KEY_MAPPING("show_ID3_tag",act_show_ID3_tag)
	     CHECK_KEY_MAPPING("search",act_search);
	     if (action_code == -1) { printf("* Unrecognised action identifier on line %d: %s\n",linenum,thetoken);
	     founderror = true; }
	     else 
              { while((thetoken = strtok(NULL," \t\r\n")))
		 { if (strlen(thetoken) == 1)
                   { MAPKEY(thetoken[0],action_code);
                   }
                 }
              }
             break;
	   case settings :
             if (!strcasecmp(thetoken,"theme"))
              { if (!theme_name_set)
                  { thetoken = strtok(NULL,"\t\r\n");
	            strcpy(theme_name, thetoken);
                  }
              }
	     else if (!strcasecmp(thetoken, "volume"))
              { if (!initial_volume_set)
                  { thetoken = strtok(NULL,"\t\r\n");
	            initial_volume = atoi(thetoken);
                  }
              }
             else 
              { CHECK_BOOLEAN_SETTING("auto_play",autoplay,autoplay_set)
		CHECK_BOOLEAN_SETTING("auto_shuffle",autoshuffle,autoshuffle_set)
                CHECK_BOOLEAN_SETTING("auto_repeat",repeat,repeat_set)
                CHECK_BOOLEAN_SETTING("auto_alpha",autoalpha,autoalpha_set)
                CHECK_BOOLEAN_SETTING("full_path",fullpath,fullpath_set)
                CHECK_BOOLEAN_SETTING("time_remaining",time_remaining,time_remaining_set)
                CHECK_BOOLEAN_SETTING("ID3v1",ID3v1,ID3v1_set)
		CHECK_JUMP_DIR("jump_dir1",jump_dir1)
		CHECK_JUMP_DIR("jump_dir2",jump_dir2)
		CHECK_JUMP_DIR("jump_dir3",jump_dir3)
		CHECK_JUMP_DIR("jump_dir4",jump_dir4)
		CHECK_JUMP_DIR("jump_dir5",jump_dir5)
		CHECK_JUMP_DIR("jump_dir6",jump_dir6)
		CHECK_JUMP_DIR("jump_dir7",jump_dir7)
		CHECK_JUMP_DIR("jump_dir8",jump_dir8)
		CHECK_JUMP_DIR("jump_dir9",jump_dir9)
		CHECK_JUMP_DIR("jump_dir0",jump_dir0)
                if (!strcasecmp(thetoken,"mpg123_params"))
                 { if (!mpg123params_set)
                     { char *foo = strtok(NULL,"\r\n");
                       if (foo) strcpy(mpg123params,foo);
		       mpg123params[255] = '\0';
                     }
                 }
                else if (!strcasecmp(thetoken, "mpg123_priority"))
                 { if (!priority)
                     { char *foo = strtok(NULL,"\t\r\n");
                       if (foo) priority = atoi(foo);
                     }
                 }
                else
                 { printf("* Unrecognised setting name on line %d: %s\n",linenum,thetoken);
	     founderror = true; }
             }
             break;
           case theme :
             CHECK_THEME_ELEMENT("plain",0)
             CHECK_THEME_ELEMENT("borders",1)
             CHECK_THEME_ELEMENT("text_normal",2)
             CHECK_THEME_ELEMENT("text_active",3)
             CHECK_THEME_ELEMENT("song_normal",4)
             CHECK_THEME_ELEMENT("song_playing",5)
             CHECK_THEME_ELEMENT("song_hilighted",6)
             CHECK_THEME_ELEMENT("song_playing_hilighted",7)
             CHECK_THEME_ELEMENT("dir_normal",8)
             CHECK_THEME_ELEMENT("dir_hilighted",9)
             CHECK_THEME_ELEMENT("text_help",10)
	     CHECK_THEME_ELEMENT("text_playlist_tail",11);
             if (element_code == -1) { printf("* Unrecognised theme element on line %d: %s\n",linenum,thetoken);
	     founderror = true; }
             else
              { int color_code = -1;
                for(int i = 0; i < 2; i++)
                 { thetoken = strtok(NULL," \t\r\n");
		   CHECK_COLOR_CODE("black",COLOR_BLACK)
                   CHECK_COLOR_CODE("red",COLOR_RED)
                   CHECK_COLOR_CODE("green",COLOR_GREEN)
                   CHECK_COLOR_CODE("yellow",COLOR_YELLOW)
                   CHECK_COLOR_CODE("blue",COLOR_BLUE)
                   CHECK_COLOR_CODE("magenta",COLOR_MAGENTA)
                   CHECK_COLOR_CODE("cyan",COLOR_CYAN)
                   CHECK_COLOR_CODE("white",COLOR_WHITE);
		   if (color_code == -1) { printf("* Unrecognised color code on line %d: %s\n",linenum,thetoken);
		   founderror = true; }
                   else
                    { color[element_code][i] = color_code;
                    }
                 }
                thetoken = strtok(NULL," \t\r\n");
		if (thetoken == NULL) color[element_code][2] = A_NORMAL;
                else if (!strcasecmp(thetoken,"normal"))
                  color[element_code][2] = A_NORMAL;
                else if (!strcasecmp(thetoken,"bold")) 
                  color[element_code][2] = A_BOLD;
                else if (!strcasecmp(thetoken,"reverse"))
                  color[element_code][2] = A_REVERSE;
                else color[element_code][2] = A_NORMAL;
              }
             break;
         }
      }
   }
  fclose(file);
  if (!foundtheme) { printf("* Theme '%s' not found - using defaults!\n",theme_name); founderror = true; }
  if (founderror)
   { printf("\nErrors were found in rc file, press <enter>...");
     while (getchar() != 10);
     printf("\n");
   }
  return true;
} // load_rcfile

void load_rcdefaults()
/* Default rc settings to load when no rc file exists */
{ MAPKEY('Z',act_previous_song);
  MAPKEY('Y',act_previous_song);
  MAPKEY('X',act_play_song);
  MAPKEY('C',act_pause_song);
  MAPKEY('V',act_stop_song);
  MAPKEY('B',act_next_song);
  MAPKEY('N',act_rewind_song);
  MAPKEY('M',act_fast_forward_song);
  MAPKEY('>',act_increase_volume);
  MAPKEY('.',act_increase_volume);
  MAPKEY('+',act_increase_volume);
  MAPKEY('=',act_increase_volume);
  MAPKEY('<',act_decrease_volume);
  MAPKEY(',',act_decrease_volume);
  MAPKEY('-',act_decrease_volume);
  MAPKEY('_',act_decrease_volume);
  MAPKEY('S',act_toggle_shuffle);
  MAPKEY('R',act_toggle_repeat);
  MAPKEY('F',act_toggle_full_path);
  MAPKEY('T',act_toggle_time_remaining);
  MAPKEY('A',act_toggle_alpha);
  MAPKEY('/',act_jump_root);
  MAPKEY('~',act_jump_home);
  MAPKEY('1',act_jump_user1);
  MAPKEY('2',act_jump_user2);
  MAPKEY('3',act_jump_user3);
  MAPKEY('4',act_jump_user4);
  MAPKEY('?',act_help);
  MAPKEY('H',act_help);
  MAPKEY('Q',act_quit);
  MAPKEY('/',act_search);
} // load_rcdefaults

void load_rcbasic()
/* Basic (compulsory) rc settings to load always */
{ 
#ifdef COLOR_SCREEN
  // Winamp-style colors
  SETCOLOR(0,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(1,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(2,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(3,COLOR_GREEN,COLOR_BLACK,A_BOLD);
  SETCOLOR(4,COLOR_GREEN,COLOR_BLACK,A_BOLD);
  SETCOLOR(5,COLOR_WHITE,COLOR_BLACK,A_BOLD);
  SETCOLOR(6,COLOR_GREEN,COLOR_BLUE,A_BOLD);
  SETCOLOR(7,COLOR_WHITE,COLOR_BLUE,A_BOLD);
  SETCOLOR(8,COLOR_GREEN,COLOR_BLACK,A_NORMAL);
  SETCOLOR(9,COLOR_GREEN,COLOR_BLUE,A_NORMAL);
  SETCOLOR(10,COLOR_YELLOW,COLOR_BLACK,A_BOLD);
  SETCOLOR(11,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
#else
// Mono colors
  SETCOLOR(0,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(1,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(2,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(3,COLOR_WHITE,COLOR_BLACK,A_BOLD);
  SETCOLOR(4,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(5,COLOR_WHITE,COLOR_BLACK,A_BOLD);
  SETCOLOR(6,COLOR_WHITE,COLOR_BLACK,A_REVERSE);
  SETCOLOR(7,COLOR_BLACK,COLOR_BLACK,A_REVERSE);
  SETCOLOR(8,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
  SETCOLOR(9,COLOR_WHITE,COLOR_BLACK,A_REVERSE);
  SETCOLOR(10,COLOR_WHITE,COLOR_BLACK,A_BOLD);
  SETCOLOR(11,COLOR_WHITE,COLOR_BLACK,A_NORMAL);
#endif

  MAPKEY(ERR,act_nothing);
  MAPKEY(' ',act_next_song);
  MAPKEY(KEY_UP,act_up);
  MAPKEY(KEY_DOWN,act_down);
  MAPKEY(KEY_HOME,act_home);
  MAPKEY(KEY_END,act_end);
  MAPKEY(KEY_PPAGE,act_page_up);
  MAPKEY(KEY_NPAGE,act_page_down);
  MAPKEY(KEY_LEFT,act_rewind_song);
  MAPKEY(KEY_RIGHT,act_fast_forward_song);
  MAPKEY(KEY_RESIZE,act_resize);
  MAPKEY(10,act_play_selected); // Enter
  MAPKEY(12,act_redraw);        // ^L
  MAPKEY(18,act_redraw);        // ^R
  MAPKEY(263,act_jump_updir);   // Backspace
} // load_rcbasic

void usage()
/* Print usage information */
{ printf("playmp3list V %s Copyright(C) by Paul Urban (urban@rucus.ru.ac.za)\n",VERSION);
  printf("- Browse and play MP3 files in an existing playlist or directory.\n");
  printf("Usage:\n\tplaymp3list [options] <playlist>\n");
  printf("\tplaymp3list [options] <directory>\n");
  printf("\tplaymp3list [options] <song> [<song>...]\n\n");
  printf("Options:\n");
  printf("\t-p\tAuto start playing\n");
  printf("\t-s\tAuto shuffle\n");
  printf("\t-r\tAuto repeat\n");
  printf("\t-t\tDisplay time remaining\n");
  printf("\t-a\tAlphabetise playlist\n");
  printf("\t-f\tDisplay full path in playlist\n");
  printf("\t-i\tLoad ID3v1 tags on startup\n");
  printf("\t-d\tSave debug info to ~/playmp3list.debug\n");
  printf("\t-T <x>\tUse a particular theme\n");
  printf("\t-P <x>\tSpecify mpg123 parameters\n");
  printf("\t-N <x>\tSpecify mpg123 process priority\n");
  printf("\t-V <x>\tSpecify initial volume\n");
  printf("\nEdit your ~/.playmp3listrc or /etc/playmp3listrc file for additional configuration options. Press '?' in the program for the list of default control keys.\n");
} // usage

int main(int argc, char *argv[])
{ if (argc < 2)
    { usage(); return(1); }

  signal(SIGSEGV,panic);
  signal(SIGINT,panic);
  signal(SIGTERM,panic);
  signal(SIGWINCH,on_sigwinch);

  setlocale(LC_ALL, "");

  strcpy(mpg123params,"");
  strcpy(currsongname,"");
  strcpy(prevdirname,"");

  int ch;
  while ((ch = getopt(argc, argv, "psrtafidT:P:N:V:")) != -1) 
    { switch (ch) 
        { case 'p' : autoplay = true; autoplay_set = true; break;
          case 's' : autoshuffle = true; autoshuffle_set = true; break;
          case 'r' : repeat = true; repeat_set = true; break;
          case 't' : time_remaining = true; time_remaining_set = true; break;
          case 'a' : autoalpha = true; autoalpha_set = true; break;
          case 'f' : fullpath = true; fullpath_set = true; break;
          case 'i' : ID3v1 = true; ID3v1_set = true; break;
          case 'd' : save_debug = true; save_debug_set = true; break;
          case 'T' : strcpy(theme_name,optarg); theme_name_set = true; break;
          case 'P' : strcpy(mpg123params, optarg); mpg123params_set = true; break;
          case 'N' : priority = atoi(optarg); priority_set = true; break;
          case 'V' : initial_volume = atoi(optarg); initial_volume_set = true; break;
          case '?' :
          default  : printf("* Unrecognised command-line option.\n\n");
                     usage(); return(1);
        }
    }

  /* load playmp3listrc settings */
  load_rcbasic();
  if (!(load_rcfile()))
    { printf("No rc files found, using defaults!\n");
      load_rcdefaults();
    }

  //disregard the option arguments
   argc -= optind; //decrease argc by optind
   argv += optind; //move argv[0] forward by optind

   if (argc == 0)
     { printf("* No playlist or directory specified.");
       usage();
       return (1);
     }

  if (save_debug)
   { char debugfile[128];
     strcpy(debugfile,getenv("HOME"));
     strcat(debugfile,"/playmp3list.debug");
     if (!freopen(debugfile,"w",stderr)) // Redirect stderr to ~/playmp3list.debug
      { printf("Error opening debug file %s for writing.\n",debugfile);
        return 3;
      }
   }
  else
   { freopen("/dev/null","w",stderr); // Redirect stderr to /dev/null (throw away)
   }

  /* create objects and start main playing loop */
  playlist = new mp3playlist();
  //if (a-- == argc && playlist->guess_file_type(argv[a]) != FT_SONG)
  if (argc == 1 && playlist->guess_file_type(argv[0]) != FT_SONG)
  // Only one non-option argument that is not a song?
   { printf("Attempting to compile playlist %s...",argv[0]);
     if (!playlist->load(argv[0], ID3v1, autoalpha, fullpath))
      { printf("error!\nCheck your playlist name!\n");
	delete playlist;
        return(2);
      }
     else printf("done.\n");
   if (autoalpha_set) playlist->sort(autoalpha, fullpath);
   }
  else			// Compile playlist from arguments...
   { printf("Compiling playlist from arguments...");
     getcwd(playlist->get_listname(),255);     // set list name
     if (!(strcmp(playlist->get_listname(),"/") == 0))
      strcat(playlist->get_listname(),"/");
     strcat(playlist->get_listname(),"(virtual playlist)");

     playlist->add_item(".","./",FT_DIR);	// add standard entry
     bool addsuccess = true;
     /*while (a < argc && addsuccess)
      { if (!(addsuccess = playlist->add_item(argv[a],ID3v1)))
          printf("\n* Bad filename: %s",argv[a]);
        a++;
      };*/
     for (int i=0;i<argc&&addsuccess;i++) {
         if (!(addsuccess = playlist->add_item(argv[i],ID3v1)))
              printf("\n* Bad filename: %s",argv[i]);
     };

     if (!addsuccess)
      { printf("\n");
        delete playlist;
        return(2);
      }
     else
      { playlist->sort(autoalpha, fullpath);
        printf("done.\n");
      }
   };

  interface = new playmp3listWindow(playlist->get_listname(), playlist, color);
  player = new mpg123frontend(interface, mpg123params, priority);
  interface->set_filename("Attempting to start decoder process...");
  if (player->init_mpg123())
   { play_list();	// run main loop
   }
  else 
   { interface->set_filename("Failed - check that mpg123 is working!");
   }

  // clean up
  delete player;
  delete interface;
  delete playlist; 
  return(0);
}; // main
