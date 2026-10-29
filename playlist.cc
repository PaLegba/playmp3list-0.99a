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

 * This is the playlist class, supporting loading, shuffling etc. of playlists.
 */

#include "playmp3list.h"
#include "playlist.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <dirent.h>
#include <cstddef>

mp3playlist::mp3playlist()
/* Constructor - zeros the counters */
{ m = n = 0;			// reset entry and song counters
  s = -1;			// reset 'current song' counter
  strcpy(currdir,"");
} // mp3playlist
    
mp3playlist::~mp3playlist()
/* Destructor */
{ clear_list();
} // ~mp3playlist

void
mp3playlist::clear_list()
/* Frees memory associated with entries and zeros the counters */
{ for (int c=0; c < n; c++)
   { delete[] list[c];
     delete[] shortlist[c];
   }
  m = n = 0;			// reset entry and song counters
  s = -1;			// reset 'current song' counter
} // clear_list

bool
mp3playlist::add_item(char *plist_entry, char *pshortlist_entry, file_type_type ptypelist_entry)
/* Reserves memory for and adds another entry */
{ if (n >= PL_MAX) return false;                       // list is full - error

  list[n] = new char[strlen(plist_entry)+1];           // add list entry
    strcpy(list[n],plist_entry);
  shortlist[n] = new char[strlen(pshortlist_entry)+1]; // add shortlist entry
    strcpy(shortlist[n],pshortlist_entry);
  typelist[n] = ptypelist_entry;                       // set typelist entry
  if (ptypelist_entry == FT_SONG) index[m++] = n;      // bump song counter &
						       //  set song index entry
  n++; return true;				       // bump entry counter
} // add_item

void recode_string ( char *s)
{

unsigned char win2koi[] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,
  28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,
  53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,
  78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,
  102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,
  121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,
  140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,
  159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,
  178,179,180,181,182,183,184,185,186,187,188,189,190,191,225,226,247,231,228,
  229,246,250,233,234,235,236,237,238,239,240,242,243,244,245,230,232,227,254,
  251,253,255,249,248,252,224,241,193,194,215,199,196,197,214,218,201,202,203,
  204,205,206,207,208,210,211,212,213,198,200,195,222,219,221,223,217,216,220,
  192,209
};
 unsigned char c;

  for (size_t i = 0; i < strlen(s); i++) { c = s[i]; c = win2koi[c]; s[i] = c; }
  return ;
}

bool
mp3playlist::add_item(char *plist_entry, bool pID3v1)
/* Guess filetype and add a properly formatted entry */
{ char *fullname, *shortname, *ext, *slash;
  bool result;
  if (plist_entry[0] == '.') return false;		// 'Hidden' file
  switch (guess_file_type(plist_entry))
   { case FT_DIR      : fullname = new char[strlen(plist_entry)+1];
			strcpy(fullname,plist_entry);
			slash = strrchr(fullname,'/');
			while (slash - fullname + 1 == (ptrdiff_t)strlen(fullname))
//			while (slash-fullname+1 == abs(strlen(fullname)))
			 { slash[0] = '\0';   // strip trailing slashes
			   slash = strrchr(fullname,'/');
                         };
			shortname = new char[strlen(fullname)+2];
                        strcpy(shortname,fullname);
			strcat(shortname,"/");
                        result = add_item(fullname, shortname, FT_DIR);
                        delete[] shortname;
			delete[] fullname;
			return result;
                        break;
     case FT_SONG     : result = false;
                        if (pID3v1)	// Extract ID3v1 tag & use it
                         { char buffer[128];
			   FILE *file = fopen(plist_entry,"r");
			   if (file)
			    { fseek(file,-128,SEEK_END);
			      fread(buffer,1,128,file);
			      fclose(file);
			      if (!strncmp(buffer, "TAG", 3))
			       { char title[31],artist[31],album[31],comment[31];
#define EXTRACT(dest,src) { \
  int i;                    \
  strncpy(dest, src, 30);   \
  dest[30] = '\0';          \
  for (i=29;i>=0;i--) {     \
    if (dest[i]==' ')       \
          dest[i]='\0';     \
    else                    \
      break;                \
   }                        \
}
				 EXTRACT(title,buffer+3);
        			 EXTRACT(artist, buffer+33);
        			 EXTRACT(album, buffer+63);
				 EXTRACT(comment, buffer+97);
				 char temp[256];
  // Print the tag info in a format you like ;-)
  //sprintf (temp,"'%s' by '%s' in '%s' (%s) ",title,artist,album,comment);
  sprintf (temp,"%s - %s (%s)",artist,title,album);
       recode_string (temp);
				 result = add_item(plist_entry, temp, FT_SONG);
                               }
                            }
                         };
			if (!pID3v1 || !result) 
                         { // Generate short name from full filename
                           ext = strrchr(plist_entry,'.');
			   shortname = new char[ext-plist_entry+1];
			   strncpy(shortname,plist_entry,ext-plist_entry);
			   shortname[ext-plist_entry] = '\0';
			   slash = strrchr(shortname,'/');
			   result = add_item(plist_entry, (slash) ? slash+1 : shortname, FT_SONG);
			   delete[] shortname;
                         };
                        return result;
			break;
     case FT_PLAYLIST : ext = strrchr(plist_entry,'.');
                        shortname = new char[ext-plist_entry+1];
                        strncpy(shortname,plist_entry,ext-plist_entry);
                        shortname[ext-plist_entry] = '\0';
                        result = add_item(plist_entry, shortname, FT_PLAYLIST);
			delete[] shortname;
			return result;
                        break;
     case FT_OTHER    :
     default          : return false; break;
   }
} // add_item (2)

bool
mp3playlist::is_a_dir(char *filename)
/* Attempts to open a directory */
{ if (strlen(filename) == 0) return false;
  DIR* dir = opendir(filename);
  bool opened_dir = (dir);
  if (dir) closedir(dir);
  return(opened_dir);
} // can_opendir

int
mp3playlist::ch_dir(char *pnewdir)
/* Run a chdir and guess the new correct current directory */
{ char newdir[1024];
  strcpy(newdir, pnewdir);
  char trimdir[512]; // trim all superfluous /'s from new dir
  if (newdir[0] == '/') strcpy(trimdir, "/");
  else strcpy(trimdir, "");
  bool firstdir = true;
  char *onedir = strtok(newdir, "/");
  while (onedir)
   { if (firstdir) firstdir = false; else strcat(trimdir,"/");
     strcat(trimdir,onedir);
     onedir = strtok(NULL,"/");
   }

  int result;
  if (currdir[0] != '/')
  // first time called successfully, so grab the current dir to start with
   { result = chdir(trimdir);
     getcwd(currdir, 511);
   }
  else
   { if (trimdir[0] == '/') // absolute path
      { strcpy(currdir, trimdir);
      }
     else if (!strcmp(trimdir, ".")); // same dir
     else if (!strcmp(trimdir, "..")) // up to parent dir
      { char *slash = strrchr(currdir, '/');
        if (slash)
         { if (slash == currdir) strcpy(currdir,"/");
           else slash[0] = '\0';
         }
      }
     else // change into child directory
      { if (strcmp(currdir, "/")) strcat(currdir, "/");
        strcat(currdir, trimdir);
      }
     result = chdir(currdir);
   }
  //fprintf(stderr,"currdir: '%s'\n",currdir);
  return result;
} // ch_dir

bool
mp3playlist::load(char *filename, bool pID3v1, bool pAlpha, bool pSortByFilename)
/* Determines whether it's a dir or a playlist and opens it */
{ switch (guess_file_type(filename))
   { case FT_DIR      : return load_dir(filename, pID3v1, pAlpha, pSortByFilename); break;
     case FT_SONG     : return false; break;
     case FT_PLAYLIST : return load_playlist(filename, pID3v1, pAlpha, pSortByFilename); break;
     case FT_OTHER    :
     default          : return false; break;
   }
} // load

bool
mp3playlist::load_dir(char *dirname, bool pID3v1, bool pAlpha, bool pSortByFilename)
/* Opens dir and adds entries */
{ if (strlen(dirname) == 0) return false;
  if (ch_dir(dirname)) return false;                // could not chdir

  fprintf(stderr,"Loading dir...\n");

  DIR *dir = NULL;
  if (!(dir = opendir("."))) return(false);         // could not opendir
  struct dirent *entry = NULL;

  strcpy(listname, currdir);                          // set list name
  if (!(strcmp(listname,"/") == 0)) strcat(listname,"/");

  clear_list();
  add_item("..","../",FT_DIR);			    // add standard entry

  while ((entry = readdir(dir)))		    // add all entries
    add_item(entry->d_name, pID3v1);

  closedir(dir);

  sort(pAlpha, pSortByFilename);		    // sort list
  return(n > 0);				    // sanity check
} // load_dir

bool
mp3playlist::load_playlist(char *filename, bool pID3v1, bool, bool)
/* Opens playlist and adds entries */
{ 
  // Extract the directory the playlist is in
  char *slash = strrchr(filename,'/');
  if (slash) 
   { char *path = new char[strlen(filename)+1];
     strncpy(path,filename,slash-filename);
     path[slash-filename] = '\0';
     fprintf(stderr,"ch_dir into: '%s'\n",path);
     if (ch_dir(path)) { delete[] path; return false; } // could not chdir
     delete[] path;
   }

  fprintf(stderr,"Loading playlist '%s'...\n",filename);

  FILE *file = fopen(filename,"r");
  if (!file) return(false);                     // could not open file

  strcpy(listname, currdir);			// set list name
  strcat(listname,"/");
  strcat(listname, (slash) ? (char *)slash+1 : filename);

  clear_list();
  add_item(".","../",FT_DIR);			// add standard entry

  char line[256];
  while (fgets(line, 255, file))		// add all entries
   add_item(strtok(line,"\n\r"),pID3v1);

  fclose(file);	

  // .pls files preserve their original order; no implicit sort
  return (n > 0);				// sanity check
} // load_playlist

bool
mp3playlist::reload(bool pID3v1, bool pAlpha, bool pSortByFilename)
/* Reload the current resource list from source */
{ char *tempstr = new char[strlen(listname)+1];
  strcpy(tempstr,listname);
  fprintf(stderr, "Reloading resource: '%s'\n",tempstr);
  return load(tempstr, pID3v1, pAlpha, pSortByFilename);
  delete [] tempstr;
} // reload

char*
mp3playlist::item(int index)
/* Returns a full filename entry */
{ if (index >= 0 && index < n) return(list[index]);
  else return list[0]/*NULL*/;
} // item

char*
mp3playlist::shortitem(int index)
/* Returns a short entry */
{ if (index >= 0 && index < n) return(shortlist[index]);
  else return shortlist[0]/*NULL*/;
} // shortitem

file_type_type
mp3playlist::file_type(int index)
/* Returns an entry's filetype */
{ if (index >= 0 && index < n) return(typelist[index]);
  else return FT_OTHER;
} // file_type

file_type_type
mp3playlist::guess_file_type(char *filename)
/* Guesses a file's filetype from the extension */
{ if (is_a_dir(filename)) return FT_DIR;		// it's a dir
  char* ext = strrchr(filename,'.');
  if (!ext) return FT_OTHER;		// no extension
  if (!strcasecmp(ext,".mp3")) return FT_SONG;  	// it's a song
  if (!strcasecmp(ext,".m3u") ||
      !strcasecmp(ext,".lst") ||
      !strcasecmp(ext,".list") ||
      !strcasecmp(ext,".pls") ||
      checklist(filename)) return FT_PLAYLIST;	// it's a playlist
  else return FT_OTHER;				// it's something else
} // guess_file_type

bool
mp3playlist::checklist(char *filename)
{ FILE *file = fopen(filename,"r");
  if (!file) return(false);                     // could not open file
  char line[256];
  fgets(line, 255, file);
  if (strlen(line) < 5) return false;		// avoids a segfault
  char* ext = strrchr(strtok(line, "\r\n"),'.');
  bool check = (ext!=NULL && (!strcasecmp(ext,".mp3")));
  fclose(file);
  return check;
}

int
mp3playlist::count()
{ return(n); 
} // count

int
mp3playlist::song_count()
{ return(m);
} // song_count
    
void
mp3playlist::shuffle()
/* Shuffle all the indexes */
{ int temp, r;
  time_t t = time(NULL);
  srandom(t);			// randomize

  for(int c=0; c < m; c++)	// switch with a random index
    { r = 0+(int) ((float(m))*rand()/(RAND_MAX+1.0));
      temp = index[c];
      index[c] = index[r];
      index[r] = temp;
    }
  if (s > 0) s = 0;		// set current pointer to first index
} // shuffle

void 
mp3playlist::unshuffle()
/* Generate sequential indexes */
{ for(int c=0; c < m; c++)
  index[c] = (n-m)+c;
  if (s > 0) s = 0;		// set current pointer to first index
} // unshuffle

void 
mp3playlist::sort(bool pAlpha, bool pSortByFilename)
/* Sort entries in descending type and optionally alphabetical order */
{ char *temp;
  bool sorted;
  file_type_type temp_et;
  char **uselist;
  if (pSortByFilename) uselist = list; else uselist = shortlist;

  do // bubble-sort
    { sorted = true;
      for(int c=1; c < n-1; c++)
      if ((typelist[c] > typelist[c+1]) ||
          (pAlpha && typelist[c] == typelist[c+1] && strcmp(uselist[c],uselist[c+1]) > 0))
        { temp = shortlist[c];			// switch shortlist entries
          shortlist[c] = shortlist[c+1];
          shortlist[c+1] = temp;
          temp = list[c];			// switch list entries
          list[c] = list[c+1];
          list[c+1] = temp;
	  temp_et = typelist[c];		// switch typelist entries
	  typelist[c] = typelist[c+1];
	  typelist[c+1] = temp_et;
          /*if (c == s) s++;
          else if (c+1 == s) s--;*/
	  s = -1;
          sorted = false;
        };
    } while (!sorted);
  unshuffle(); 					// Generate standard linear
						// shuffle-index
} // sort

bool 
mp3playlist::next(bool repeat)
/* Attempt to bump pointer to next song */
{ if (s == -1) return false;
  if (s < m-1) {++s; return true; }
  else if (repeat) {s = 0; return true; }
  else return false;
} // next

bool 
mp3playlist::prev()
/* Attempt to bump pointer to previous song */
{ if (s > 0) {--s; return true; }
  else return false;
} // prev
    
int 
mp3playlist::curr()
/* Return the song pointed to through the index */
{ if (s != -1) return(index[s]); else return -1;
} // curr

void 
mp3playlist::set_s(int news)
/* Set the current pointer */
{ s = news-(n-m);
  if (s < 0) s = 0;
  else if (s >= m) s = m-1;
} // set_s

int
mp3playlist::get_s()
/* Return the current pointer */
{ return s;              // Only used in special circumstances
} // get_s

char*
mp3playlist::get_listname()
/* Return the formatted list name */
{ return listname;
} // get_listname

