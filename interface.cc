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

 * This is the ncurses interface, encapsulated in a playmp3listWindow class.
 */

#include "playmp3list.h"
#include NCURSES
#include "interface.h"
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>

playmp3listWindow::playmp3listWindow(char *pfilename, mp3playlist *pplaylist, int pcolor[12][3])
/* Constructor - creates ncurses window */
{ fprintf(stderr,"Creating ncurses interface...\n");
  playlist = pplaylist;
  initscr();
#ifdef COLOR_SCREEN
  start_color();
#endif
  cbreak();
  noecho();
  curs_set(0);
  lines = LINES;
  cols = COLS;
  pos = top = 0;
  fullpath = false;
  filename = new char[1];
  filename[0] = '\0';
  listname = new char[1];
  listname[0] = '\0';

  set_colors(pcolor);

  leaveok(stdscr, TRUE);
  keypad(stdscr, TRUE);
  draw_statics();
  set_listname(pfilename);
  refresh();
}  // playmp3listWindow

playmp3listWindow::~playmp3listWindow()
/* Destructor */
{ fprintf(stderr,"Destroying ncurses interface...\n"); 
  curs_set(1);
  bkgd(COLOR_PAIR(1));
  clear();
  refresh();
  endwin();
  if (filename) delete[] filename;
  if (listname) delete[] listname;
} // ~playmp3listWindow

void  
playmp3listWindow::draw_statics()
/* Draw static text */
{ bkgd(COLOR_PAIR(2)|style[2]); // window color = border color
  attron(style[1]);
  border(ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, ACS_ULCORNER, ACS_URCORNER,  ACS_LLCORNER, ACS_LRCORNER);
  mvaddch(lines-4, 0, ACS_LTEE);
  for (int i=1; i < cols-1; i++)
    mvaddch(lines-4, i, ACS_HLINE);
  mvaddch(lines-4, cols-1, ACS_RTEE);
  attroff(style[1]);
  
  mvaddstr(lines-2, 17, "    KHz,     Kb/s (      )");
  mvaddstr(lines-2, cols-35, "Shuffle [ ]  Repeat [ ]  Vol   0%");
  mvaddstr(lines-2, 9, "[     ]");
  mvchgat(lines-3,3,cols-5,style[3],3,NULL);
  mvchgat(lines-2,3,cols-5,style[3],3,NULL);
} // draw_statics

void
playmp3listWindow::draw_progress()
{ int start = 2;
  int end = cols - 3;
  int bar_width = end - start - 1;
  int progress = 0;
  if (totaltime > 0 && elapsedtime >= 0)
    progress = (int)((long long)elapsedtime * bar_width / totaltime);
  if (progress < 0) progress = 0;
  if (progress > bar_width) progress = bar_width;
  wchar_t filled[2] = { 0x25CF, L'\0' };
  wchar_t empty[2] = { 0x25CB, L'\0' };
  mvaddch(lines-4, start, '[' | COLOR_PAIR(2) | style[1]);
  for (int i = start + 1; i < end; i++)
  { if (i - start - 1 < progress)
      mvaddwstr(lines-4, i, filled);
    else
      mvaddwstr(lines-4, i, empty);
  }
  mvaddch(lines-4, end, ']' | COLOR_PAIR(2) | style[1]);
  mvchgat(lines-4, start + 1, bar_width, style[1], 2, NULL);
} // draw_progress

void
playmp3listWindow::show_noinfo()
/* Display blanks in interface bottom bar */
{ set_totaltime(-1);
  set_elapsedtime(-1,-1);
  set_format(-1,-1,-1);
}

void    
playmp3listWindow::set_filename(const char *pfilename)
{ if (filename) delete[] filename;
  filename = new char[strlen(pfilename)+1];
  strcpy(filename, pfilename);
  draw_filename();
  refresh();
} // set_filename

void
playmp3listWindow::draw_filename()
{ int fn_max = cols - 7;
  if (fn_max < 1) return;
  mvaddnstr(lines-3, 5, filename, fn_max);
  for (int i = 5 + (int)strlen(filename); i < cols - 2; i++)
    mvaddch(lines-3, i, ' ');
  mvchgat(lines-3, 4, cols - 6, style[5], 6, NULL);
} // draw_filename

void
playmp3listWindow::set_listname(char *plistname)
{ if (listname) delete[] listname;
  listname = new char[strlen(plistname)+1];
  strcpy(listname, plistname);
  draw_listname();
  refresh();
} // set_listname

void
playmp3listWindow::draw_listname()
{ attron(style[1]);
  for (int i=1; i < cols-1; i++)
    mvaddch(0, i, ACS_HLINE);
  char *startchar;
  char dots[4] = "...";
  bool elipses = false;
  int l = strlen(listname);
  if (l <= cols-4)
    startchar = listname;
  else
   { startchar = listname+(l-(cols-4)+3);
     l = cols-4;
     elipses = true;
   }
   mvaddch(0, ((cols-l)/2)-1, ACS_RTEE);
  mvaddch(0, (cols+l)/2, ACS_LTEE);
  attroff(style[1]);
  if (elipses)
    { mvaddnstr(0, 2, dots, cols-4);
      mvaddnstr(0, 5, startchar, cols-4);
    }
  else
    mvaddnstr(0, (cols-l)/2, startchar, cols-4);
} // draw_listname
    
void
playmp3listWindow::set_format(int pfreq, int pbitrate, bool pstereo)
{ freq = pfreq;
  bitrate = pbitrate;
  stereo = pstereo;

  draw_format();
  refresh();
} // set_format

void
playmp3listWindow::draw_format()
{ if (freq == -1)
   { mvaddstr(lines-2, 17, "    KHz,     Kb/s (      )");
     mvchgat(lines-2, 17,26,style[3],3, NULL);
   }
  else
   { mvprintw(lines-2, 17,"%3d", freq/1000);
     mvprintw(lines-2, 26,"%3d", bitrate);
     mvprintw(lines-2, 36,"%s", (stereo ? "stereo" : " mono "));
      mvchgat(lines-2, 17, 3, style[10], 11, NULL);
      mvchgat(lines-2, 26, 3, style[10], 11, NULL);
      mvchgat(lines-2, 36, 6, style[10], 11, NULL);
   }
} // draw_format

void
playmp3listWindow::set_shuffle(bool pshuffle)
{ shuffle = pshuffle;
  draw_shuffle();
  refresh();
} // set_shuffle

void
playmp3listWindow::draw_shuffle()
{ mvaddch(lines-2, cols-26, (shuffle ? 'X'|COLOR_PAIR(11)|style[10] : ' '|COLOR_PAIR(3)));
} // draw_shuffle

void
playmp3listWindow::set_repeat(bool prepeat)
{ repeat = prepeat;
  draw_repeat();
  refresh();
} // set_repeat

void
playmp3listWindow::draw_repeat()
{ mvaddch(lines-2, cols-14, (repeat ? 'X'|COLOR_PAIR(11)|style[10] : ' '|COLOR_PAIR(3)));
} // draw_repeat
      
void
playmp3listWindow::set_volume(int pvolume)
{ volume = pvolume;
  draw_volume();
  refresh();
} // set_volume

void
playmp3listWindow::draw_volume()
{ mvprintw(lines-2, cols-6, "%3d",volume);
  mvchgat(lines-2, cols-6, 3, style[10], 11, NULL);
} // draw_volume
      
void
playmp3listWindow::set_playstate(playstate_type pplaystate)
{ playstate = pplaystate;
  draw_playstate();
  refresh();
} // set_playstate

playstate_type
playmp3listWindow::get_playstate()
{ return playstate;
} // get_playstate

void
playmp3listWindow::draw_playstate()
{ wchar_t playing[] = { L' ', 0x25B6, L'\0' };
  wchar_t paused[] = { L' ', 0x23F8, L'\0' };
  wchar_t stopped[] = { L' ', 0x25A0, L'\0' };
  wchar_t noplay[] = { L' ', 0x25C7, L'\0' };
  switch(playstate)
    { case NOPLAY  : mvaddwstr(lines-3, 2, noplay); break;
      case STOPPED : mvaddwstr(lines-3, 2, stopped); break;
      case PLAYING : mvaddwstr(lines-3, 2, playing); break;
      case PAUSED  : mvaddwstr(lines-3, 2, paused); break;
    }
  mvaddch(lines-3, 4, ' ');
  mvchgat(lines-3, 2, 3, style[3], 4, NULL);
} // draw_playstate
      
void
playmp3listWindow::set_elapsedtime(int pelapsedtime, int premainingtime)
{ elapsedtime = pelapsedtime;
  remainingtime = premainingtime;
  draw_elapsedtime();
  draw_progress();
  refresh();
} // set_elapsedtime
      
void
playmp3listWindow::draw_elapsedtime()
{ char str[16];
  if (elapsedtime > -1) 
  { if (timeremaining_mode) 
      sprintf(str, "-%02d:%02d", (remainingtime) / 60, (remainingtime)%60);
    else
      sprintf(str, " %02d:%02d", elapsedtime / 60, elapsedtime%60);
  }    
  else sprintf(str, "   :  ");
  mvaddstr(lines-2, 2, str);
  mvchgat(lines-2, 2, 6, style[10], 11, NULL);
} // draw_elapsedtime

void
playmp3listWindow::set_totaltime(int time)
{ totaltime = time;
  draw_totaltime();
  draw_progress();
  refresh();
} // set_totaltime

void
playmp3listWindow::draw_totaltime()
{ char str[16];
  if (totaltime > -1) sprintf(str, "%02d:%02d", totaltime / 60, totaltime%60);
  else sprintf(str, "  :  ");
  mvaddstr(lines-2, 10, str);
  mvchgat(lines-2, 10, 5, style[10], 11, NULL);
} // draw_totaltime

void 
playmp3listWindow::scrollto(int showpos)
{ // Some code to scroll the window to show this selection
  if (top > showpos)
    { top = showpos - ((lines-5) / 2);
      if (top < 0) top = 0;
    }
  else if (top + (lines-6) < showpos)
    { top = showpos-((lines-5)/2);
      if (top+(lines-6) > playlist->count())
        top = playlist->count() - lines + 5;
    }
  draw_list();
} // scrollto

void
playmp3listWindow::set_playpos(int pplaypos)
{ playpos = pplaypos;
  scrollto(playpos);
  
  /*if (top > playpos) 
    { top = playpos - ((lines-5) / 2);
      if (top < 0) top = 0;
    }  
  else if (top + (lines-6) < playpos) 
    { top = playpos-((lines-5)/2); 
      if (top+(lines-6) > playlist->count())
        top = playlist->count() - lines + 5;
    }
  draw_list();*/
} // set_playpos

void
playmp3listWindow::set_pos(int ppos)
{ pos = ppos;
  scrollto(pos);
}

void
playmp3listWindow::set_timeremaining_mode(bool rem_mode)
{ timeremaining_mode = rem_mode;
} // set_timeremaining_mode
      
void
playmp3listWindow::draw_list()
{ int n = playlist->count();
  char *filename;
  int c;
  for (c = 0; c < lines - 5 && c+top < n; c++)
    {       if (fullpath) filename = playlist->item(c+top);
      else filename = playlist->shortitem(c+top);
      for(int cc = 0; cc < cols -4; cc++)
        mvaddch(1+c, 2+cc, ' '|COLOR_PAIR(2));
      mvaddnstr(1+c, 2, filename, cols-4);
      if (playlist->file_type(c+top) == FT_SONG)
      { if (c == pos && c+top == playpos)
          mvchgat(1+c, 2, cols-4, style[7], 8, NULL);
        else if (c == pos)
          mvchgat(1+c, 2, cols-4, style[6], 7, NULL);
        else if (c+top == playpos)
          mvchgat(1+c, 2, cols-4, style[5], 6, NULL);
        else mvchgat(1+c, 2, cols-4, style[4], 5, NULL);
      }
      else
      { if (c == pos)
          mvchgat(1+c, 2, cols-4, style[9], 10, NULL);
        else mvchgat(1+c, 2, cols-4, style[8], 9, NULL);
      }
    }
  while(c < lines - 5)
  { for(int cc = 0; cc < cols -4; cc++)
        mvaddch(1+c, 2+cc, ' '|COLOR_PAIR(12));
    c++;
  }    
  refresh();
} // draw_list

void
playmp3listWindow::erase_list()
/* Draw blank lines over playlist */
{ for(int y = 1; y < lines-4; y++)
  for(int x = 2; x < cols-2; x++)
    mvaddch(y, x, ' '|COLOR_PAIR(2));
  //refresh();
} // erase_list

void
playmp3listWindow::draw_help()
{ erase_list();
  mvaddstr( 1, 2, "DEFAULT CONTROL KEYS");
  mvaddstr( 2, 2, "--------------------");
  mvaddstr( 3, 2, "Z or Y\t\tPrevious");
  mvaddstr( 4, 2, "X\t\t\tPlay");
  mvaddstr( 5, 2, "C\t\t\tPause");
  mvaddstr( 6, 2, "V\t\t\tStop");
  mvaddstr( 7, 2, "B or SPACE\t\tNext");
  mvaddstr( 8, 2, "UP, DOWN etc.\t\tMove up/down playlist");
  mvaddstr( 9, 2, "ENTER\t\t\tPlay hilighted song/ browse dir/playlist");
  mvaddstr(10, 2, "LT, RT or N, M\tRewind/fast-forward current song");
  mvaddstr(11, 2, "S, R\t\t\tToggle shuffle/repeat mode");
  mvaddstr(12, 2, "F\t\t\tToggle full/short filename display");
  mvaddstr(13, 2, "T\t\t\tToggle time elapsed/remaining display");
  mvaddstr(14, 2, "A\t\t\tToggle alphabetisisation of playlist");
  mvaddstr(15, 2, "I\t\t\tDisplay ID3v1 tag info (if present) of hilighted song");
  mvaddstr(16, 2, "< > or - +\t\tDecrease/increase volume by 1%");
  mvaddstr(17, 2, "/, ~, BKSP\t\tJump to root/home/parent directory");
  mvaddstr(18, 2, "` <key>\t\tSearch for next song starting with a letter");
  mvaddstr(19, 2, "^L or ^R\t\tRedraw window");
  mvaddstr(20, 2, "Q\t\t\tQuit");
  for(int i=1; i <= 20; i++)
    mvchgat(i, 2, cols-4, style[11], 11, NULL);
  refresh();
} // draw_help

void
playmp3listWindow::draw_info(char **line)
{ erase_list();
  int c = 0;
  while (line[c] && strlen(line[c]) > 0)
   { mvaddstr( c+1, 2, line[c]); c++; };
  for(int i=1; i <= c; i++)
    mvchgat(i, 2, cols-4, style[11], 11, NULL);
  refresh();
} // draw_ID3v1_info

void
playmp3listWindow::action_next(chtype firstchar)
{ int orginalpos = pos+top; 
  char *afilename;
  do
   { if(++pos > lines - 6)
      { pos = lines - 6;
        top++;
      }
     if(pos+top >= playlist->count())
      { top = pos = 0;
      }
     if (fullpath) afilename = playlist->item(pos+top);
     else afilename = playlist->shortitem(pos+top);
     if (toupper(*afilename)==toupper(firstchar)) break;
   } while (pos+top!=orginalpos);
  draw_list();
} // action_next

void 
playmp3listWindow::action_up()
{ if(--pos < 0)
    { pos = 0;
      if (top > 0) top--; 
    };
    draw_list();
} // action_up

void
playmp3listWindow::action_down()
{  if(++pos > lines - 6)
    { pos = lines - 6;
      if (pos+top < playlist->count()-1) top++;
    }
   else if(pos+top >= playlist->count()) pos--;
    draw_list();
} // action_down

void
playmp3listWindow::action_home()
{ pos = top = 0;
  draw_list();
} // action_home

void
playmp3listWindow::action_end()
{ top = playlist->count()-lines+5;
  if (top < 0)
    { top = 0;
      pos = playlist->count()-1;
    }
  else
    { pos = lines-6; }
  draw_list();
} // action_end

void
playmp3listWindow::action_pgup()
{ if (pos > 0) pos = 0;
  else
    { top -= lines - 6;
      if (top < 0) top = 0;
    }
  draw_list();
} // action_pgup

void
playmp3listWindow::action_pgdn()
{ if (pos < lines - 6) 
    { pos = lines - 6;
      if (pos > playlist->count()-1)
        pos = playlist->count()-1;
    }
  else
    { top += lines - 6;
      if (pos + top > playlist->count())
        top = playlist->count() - lines + 5;
    }
  draw_list();
} // action_pgdn

int
playmp3listWindow::get_listpos()
{ return(pos+top);
} // get_listpos

int
playmp3listWindow::get_ch()
{ int ch;
  fd_set fdsr;
  struct timeval tv;

  fflush(stdin);
  FD_ZERO(&fdsr);
  FD_SET(0, &fdsr);  /* stdin file descriptor */
  tv.tv_sec = tv.tv_usec = 0;
  if (select(FD_SETSIZE, &fdsr, NULL, NULL, &tv) == 1)
   ch = getch();
  else
   ch = (chtype) ERR;

  return(ch);						  
} // get_ch

void
playmp3listWindow::resize()
{ endwin();
  refresh();
  lines = LINES;
  cols = COLS;
  clear();
  draw_statics();
  draw_listname();
  draw_playstate();
  draw_filename();
  draw_elapsedtime();
  draw_totaltime();
  draw_progress();
  draw_format();
  draw_shuffle();
  draw_repeat();
  draw_volume();
  draw_list();
} // resize

void
playmp3listWindow::do_refresh()
{ /*touchwin(stdscr);
  refresh();*/  // I don't know why this doesn't work, so I'm using this instead...
  resize();
} // do_refresh

void
playmp3listWindow::set_colors(int pcolor[12][3])
{ for (int c = 0; c < 12; c++)
   { init_pair(c+1, pcolor[c][0], pcolor[c][1]);
     style[c] = pcolor[c][2];
   }
} // set_colors

