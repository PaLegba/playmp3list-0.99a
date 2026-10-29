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

 * This is the decoder frontend class. It send messages to and processes
 * messages from an mpg123 child process. It also manages the creation and
 * destruction of these processes.
 */

#include "frontend.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/timeb.h>

extern void recode_string (char *s);

mpg123frontend::mpg123frontend(playmp3listWindow *pinterface, char* pparams, int ppriority)
/* Constructor */
{ playstate = NOPLAY;
  interface = pinterface;
  connected = initialised = false;
  stopping = false;
  priority = ppriority;
  num_inits = 0;
  mpg123_pid = 0;
  seekvalue = 16;

  // Compile a list of parameters from the parameter string
#define add_param(s) { \
 param_list[c] = new char[strlen(s)+1]; \
 strcpy(param_list[c++],s); \
}
  int c = 0;

  ftime(&seeklast);

  add_param("mpg123");
  add_param("-R");
  char *this_param;
  if ((this_param = strtok(pparams," \t\r\n")))
   { do
      { add_param(this_param);
      } while ((c < paramlen-3) && (this_param = strtok(NULL," \t\r\n")));
   }
  add_param("-");
} // mpg123frontend
      
mpg123frontend::~mpg123frontend()
/* Destructor */
{ if (initialised) killmpg123();
  for (int c = 0; c < paramlen-2 && param_list[c]; c++) delete[] param_list[c];
} // ~mpg123frontend

int 
mpg123frontend::readline()
/* Append newly written character(s) to buffer 
   returns -1 if stream is closed, true if a complete string has been
   read in, and false otherwise */
{ fd_set fdsr; 
  struct timeval tv;
  FD_ZERO(&fdsr);
  FD_SET(mpg123_recv, &fdsr);
  tv.tv_sec = tv.tv_usec = 0;
  if (select(mpg123_recv+1, &fdsr, NULL, NULL, &tv) == 1) // Has mpg123 said anything?
   { int rc;
     while(1)
      { rc = read(mpg123_recv, buf+buflen, 1);
        if (rc <= 0) return rc-1; // Stream closed = process died?
        if (rc == 0) return false;  // EOF - not more chars yet
        if (buf[buflen] == '\0' || buf[buflen] == '\n' || buflen == BUF_SIZE)
         { buf[buflen] = '\0';
           buflen = 0;
           return true;
         };
        buflen += 1;
      }
   };
  return false;
} // readline

char *
mpg123frontend::trimleft (char *str)
{ int c;
  while ((str[0]==' ' || str[0]=='\t') && str[0] != '\0')
   { c = 0;
     do
      { str[c] = str[c+1];
      } while (str[++c] != '\0');
   }  
  return str;
} // trimleft

char *
mpg123frontend::trimright (char *str)
{ char *s = str + strlen(str) - 1;
  while (s>=str && (*s==' ' || *s=='\t'))
   *s-- = '\0';
  return str;
} // trimright

char *
mpg123frontend::trim (char *str)
{ return trimleft(trimright(str));
} // trim

void
mpg123frontend::killmpg123()
/* Closes comm lines and kills mpg123 child process */
{ if (mpg123_pid > 0)
   { if (mpg123_send) close(mpg123_send);
     if (mpg123_recv) close(mpg123_recv);
     mpg123_send = mpg123_recv = 0;
     fprintf(stderr,"Killing mpg123 process, pid %d\n", mpg123_pid);
     kill(mpg123_pid,SIGTERM);
     connected = initialised = false;
     mpg123_pid = 0;
     /*interface->set_playstate(NOPLAY);
     interface->set_filename("");*/
     interface->set_playstate(STOPPED);
   }
} // killmpg123

bool
mpg123frontend::init_mpg123()
/* Sets up comm sockets, forks a new process, and starts mpg123 */
{ if (initialised) return true;
  stopping = false;
  fprintf(stderr,"Preparing to fork mpg123 child process...\n");

  int fd_send[2], fd_recv[2];

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd_send) < 0)
    return false;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd_recv) < 0)
    return false;
  
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN); // Ignore signal for death of the child process

  mpg123_pid = fork();

  if (mpg123_pid == 0) // I am a child!
   { // pipe in/output through socket to parent
     dup2(fd_send[0], STDIN_FILENO);
     close(fd_send[0]);
     close(fd_send[1]);
     dup2(fd_recv[0], STDOUT_FILENO);
     dup2(fd_recv[0], STDERR_FILENO);
     close(fd_recv[0]);
     close(fd_recv[1]);

     // start mpg123 process
     if (execvp("mpg123",param_list) < 0) return false;
     exit(-1);
   };

  // I am a parent!
  close(fd_send[0]);
  mpg123_send = fd_send[1];
  close(fd_recv[0]);
  mpg123_recv = fd_recv[1];

  usleep(500);  // wait a bit (500 msec)
  if (mpg123_pid == -1) // child is dead
   { fprintf(stderr,"Parent process: child is dead\n");
     if (mpg123_send) close(mpg123_send);
     if (mpg123_recv) close(mpg123_recv);
     mpg123_send = mpg123_recv = 0;
     return false;
   }
  else // child is alive
   { connected = initialised = true; 
     fprintf(stderr,"Parent process: child is running, pid %d\n",mpg123_pid);
     buflen = 0;
     fprintf(stderr,"Attempting to renice the decoder process to %d...\n",priority);
     setpriority(PRIO_PROCESS,mpg123_pid,priority);
     return true; 
   }
} // init_mpg123

bool
mpg123frontend::process_messages(bool &endofsong)
/* Retrieves messages from player mpg123 process and processes them
   returns false if the mpg123 process has ended */
{ usleep(1); // This reduces the 'playmp3list' thread's CPU use DRAMATICALLY!
  if (!initialised) return true;
  int rc = readline();
  if (rc < 0) { killmpg123(); return false; } // Process has ended
  else if (rc == true) // A full line has been received
   { char *c1, *c2, *c3, *c4, *c5, *c6;
     int ps;
     fprintf(stderr,"Read mpg123 message: %s\n",buf);
     c1 = strtok(buf, " \t");
     if (c1 && !strcasecmp(c1, "@F")) // It's Frame & time info
      { c2 = c1 ? strtok(NULL, " \t") : NULL;
        c3 = c2 ? strtok(NULL, " \t") : NULL;
        c4 = c3 ? strtok(NULL, " \t") : NULL;
        c5 = c4 ? strtok(NULL, " \t") : NULL;
	c6 = c5 ? strtok(NULL, " \t") : NULL;
	last_frameremain = atoi(c3);
        int totaltime = (int)(atof(c4)+atof(c5));
        if (last_totaltime == -2 || totaltime < last_totaltime - 1 || totaltime > last_totaltime + 1)
         { last_totaltime = totaltime;
	   interface->set_totaltime(totaltime);
         }
        interface->set_elapsedtime(atoi(c4),atoi(c5));
      }
     else if (c1 && !strcasecmp(c1, "@P")) // It's playstate info
      { c2 = c1 ? strtok(NULL, " \t") : NULL;
        ps = atoi(c2);
	switch (ps)
         { /*case 0 : if (last_frameremain < 3)
		     { endofsong = true;
 		       interface->set_playstate(STOPPED);
		       interface->set_elapsedtime(-1,-1);
		     }
		    else
		     { fprintf(stderr, "last_frameremain on stop: %d.\n",last_frameremain);
                       interface->set_playstate(STOPPED);
		       interface->set_elapsedtime(-1,-1);
		     } break;*/
	   case 0 : { endofsong = !stopping;
	              stopping = false;
		      interface->set_playstate(STOPPED);
		      interface->set_elapsedtime(-1,-1);
		    } break;
           case 1 : interface->set_playstate(PAUSED); break;
           case 2 : interface->set_playstate(PLAYING); break;
         }
      }
      else if (c1 && !strcasecmp(c1, "@I")) // It's song info
       { interface->set_playstate(PLAYING);
         last_totaltime = -2;
         c2 = c1 ? strtok(NULL, "") : NULL;
          if (!c2) return true;
         // Strip trailing \r
         { char *cr = strchr(c2, '\r'); if (cr) *cr = '\0'; }
         fprintf(stderr, "RAW @I: '%s'\n", c2);
         // Skip leading non-alphanumeric garbage
         while (c2[0] && (c2[0] == '}' || c2[0] == '{' || c2[0] == '\n' || c2[0] == '\r'))
           c2++;
         if (!c2[0]) return true;
         if (!strncasecmp(c2, "ID3:", 4))
          { c2 += 4;
            char info_title[31], info_artist[31], info_album[31], info_year[5], info_comment[31], info_genre[31];
#define extract(s, l) { \
 strncpy(s,c2,l); s[l] = '\0'; trim(s); c2+=l; \
}
           extract(info_title,30);
           extract(info_artist,30);
           extract(info_album,30);
           extract(info_year,4);
           extract(info_comment,30);
           extract(info_genre, 30);
           // compile a nice name from the ID3 tag info
           char *gen_filename = new char[strlen(info_artist)+strlen(info_title)+strlen(info_album)+7];
           sprintf(gen_filename, "%s - %s (%s)",info_artist,info_title,info_album);
           recode_string (gen_filename);
           interface->set_filename(gen_filename);
         }
         else if (!strncasecmp(c2, "ID3v2.", 6))
          { have_id3v2_tags = true;
           char *tag = c2 + 6;
           char *colon = strchr(tag, ':');
            if (colon)
             { bool known_tag = true;
               *colon = '\0';
               char *value = colon + 1;
               { char *p = value + strlen(value) - 1;
                 while (p >= value && (*p == ' ' || *p == '\t')) *p-- = '\0'; }
                if (!strcasecmp(tag, "TIT2") || !strcasecmp(tag, "TT2") || !strcasecmp(tag, "title"))
                 strncpy(id3v2_title, value, 255);
                else if (!strcasecmp(tag, "TPE1") || !strcasecmp(tag, "TP1") || !strcasecmp(tag, "artist"))
                 strncpy(id3v2_artist, value, 255);
                else if (!strcasecmp(tag, "TALB") || !strcasecmp(tag, "TAL") || !strcasecmp(tag, "album"))
                 strncpy(id3v2_album, value, 255);
                else
                 known_tag = false;
               if (known_tag) build_id3v2_name();
             }
         }
        else // use the basic name, sanitized
         { char cleaned[512]; int j = 0;
           for (int i = 0; c2[i] != '\0' && j < 510; i++)
            { unsigned char ch = c2[i];
              if (ch >= 0x20 && ch <= 0x7E) cleaned[j++] = ch;
              else if (ch >= 0x80) cleaned[j++] = ch; }
           cleaned[j] = '\0';
           interface->set_filename(cleaned);
         }
      }
     else if (c1 && !strcasecmp(c1, "@S")) // It's stream info
      { int frequency, bitrate, stereo;
        c2 = c1 ? strtok(NULL, " \t") : NULL; // mpeg_type
        c2 = c2 ? strtok(NULL, " \t") : NULL; // layer
        frequency = atoi(c2 = c2 ? strtok(NULL, " \t") : NULL); // frequency
	c2 = c2 ? strtok(NULL, " \t") : NULL; // mode
	c2 = c2 ? strtok(NULL, " \t") : NULL; // mode extension
	c2 = c2 ? strtok(NULL, " \t") : NULL; // framesize
	stereo = atoi(c2 = c2 ? strtok(NULL, " \t") : NULL); // stereo
	c2 = c2 ? strtok(NULL, " \t") : NULL; // copyright
	c2 = c2 ? strtok(NULL, " \t") : NULL; // error protection
	c2 = c2 ? strtok(NULL, " \t") : NULL; // emphasis
	bitrate = atoi(c2 = c2 ? strtok(NULL, " \t") : NULL); // bitrate
	c2 = c2 ? strtok(NULL, " \t") : NULL; // extension
	interface->set_format(frequency, bitrate, stereo);
      }
     else if (c1 && !strcasecmp(c1,"@R"))
      { //connected = true;
        c2 = c1 ? strtok(NULL, "") : NULL;
        char startmessage[512];

	/*if (num_inits++) sprintf(startmessage,"Decoder '%s' re-started.",c2);
        else sprintf(startmessage, "Decoder '%s' started. (Press ? for list of default keys).",c2);
        interface->set_filename(startmessage);*/

	if (!num_inits++) 
         { sprintf(startmessage,"Decoder '%s' started. (Press ? for list of default keys).",c2);
           interface->set_filename(startmessage);
         }
      }
     else // Unrecognised message (probably error message)
      { c2 = c1 ? strtok(NULL, "") : NULL;
        char mpg123message[512];
        sprintf(mpg123message,"Decoder: %s",c2);
        //interface->set_filename(mpg123message); // Let's not display it - probably just messy
      }
   }
  return true;
} // process_messages

void
mpg123frontend::open_file(char *filename)
/* Open a song file for playing */
{ if (!initialised) init_mpg123();
  if (!connected) return;
  clear_id3v2();
  char s[512];
  sprintf(s, "LOAD %s\n",filename);
  fprintf(stderr,"Sent mpg123 message: %s",s);
  write(mpg123_send,s,strlen(s));
} // open_file

void
mpg123frontend::clear_id3v2()
{ id3v2_title[0] = '\0';
  id3v2_artist[0] = '\0';
  id3v2_album[0] = '\0';
  have_id3v2_tags = false;
} // clear_id3v2

void
mpg123frontend::build_id3v2_name()
{ if (!have_id3v2_tags) return;
  char name[1024]; name[0] = '\0';
  bool has_artist = (id3v2_artist[0] != '\0');
  bool has_title = (id3v2_title[0] != '\0');
  bool has_album = (id3v2_album[0] != '\0');
  if (has_artist && has_title)
   { if (has_album)
       sprintf(name, "%s - %s (%s)", id3v2_artist, id3v2_title, id3v2_album);
     else
       sprintf(name, "%s - %s", id3v2_artist, id3v2_title);
   }
  else if (has_title)
   { if (has_album)
       sprintf(name, "%s (%s)", id3v2_title, id3v2_album);
     else
       sprintf(name, "%s", id3v2_title);
   }
  else if (has_artist)
   { sprintf(name, "%s", id3v2_artist); }
  else if (has_album)
   { sprintf(name, "%s", id3v2_album); }
  if (name[0])
   interface->set_filename(name);
} // build_id3v2_name
      
playstate_type
mpg123frontend::get_state()
{ return(playstate);
} // get_state

void
mpg123frontend::action_play()
{ if (!initialised) init_mpg123();
  if (!connected) return;
} // action_play

void
mpg123frontend::action_stop()
{ if (!connected) return;
  fprintf(stderr,"Sent mpg123 message: STOP\n");
  stopping = true;
  write(mpg123_send,"STOP\n",5);
  killmpg123();
} // action_stop

void
mpg123frontend::action_pause()
{ if (!connected) return;
  fprintf(stderr,"Sent mpg123 message: PAUSE\n");
  write(mpg123_send,"PAUSE\n",6);
} // action_pause
    
void
mpg123frontend::action_ffwd()
{ if (!connected) return;
  /*fprintf(stderr,"Sent mpg123 message: JUMP +32\n");
  write(mpg123_send,"JUMP +32\n",9);*/
  struct timeb seeknow;
  char jumpstr[15];

  ftime(&seeknow);
  if (seeknow.time==seeklast.time || seeknow.time-1==seeklast.time)
    { if (seeknow.millitm+1000*(seeknow.time-seeklast.time)-seeklast.millitm<100)
      seekvalue=seekvalue+1;
    }
  else seekvalue=16;
  seeklast=seeknow;
  sprintf(jumpstr,"JUMP +%d\n",seekvalue);
  fprintf(stderr,"Sent mpg123 message: %s",jumpstr);
  write(mpg123_send,jumpstr,strlen(jumpstr));
} // action_ffwd
    
void
mpg123frontend::action_rwnd()
{ if (!connected) return;
  /*fprintf(stderr,"Sent mpg123 message: JUMP -32\n");
  write(mpg123_send,"JUMP -32\n",9);*/
  struct timeb seeknow;
  char jumpstr[15];

  ftime(&seeknow);
  if (seeknow.time==seeklast.time || seeknow.time-1==seeklast.time)
    { if (seeknow.millitm+1000*(seeknow.time-seeklast.time)-seeklast.millitm<100)
      seekvalue=seekvalue+1;
    }
  else seekvalue=16;
  seeklast=seeknow;
  sprintf(jumpstr,"JUMP -%d\n",seekvalue);
  fprintf(stderr,"Sent mpg123 message: %s",jumpstr);
  write(mpg123_send,jumpstr,strlen(jumpstr));
} // action_rwnd

void
mpg123frontend::set_volume(int pct)
{ if (!connected) return;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "VOLUME %d\n", pct);
  write(mpg123_send, cmd, strlen(cmd));
} // set_volume
