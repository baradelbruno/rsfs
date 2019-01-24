/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096
#define FATSECTOR 256
#define DIRSIZE 8

unsigned short fat[65536];
unsigned short aberto[128];
char tempbuffer[4096];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

typedef struct {
  int aberto;
  int posbuffer;
  int blocoatual;
  int nbytes;
  int sector;
} filesave;

dir_entry dir[128];
filesave file_save[128];
filesave file_read[128];

int formatado = 1; //variável global pra facilitar a checagem do disco

void write_fat() {
  char *f = (char *) fat;
  int i;
  for(i = 0; i < FATSECTOR; i++) {
    bl_write(i, f);
    f += SECTORSIZE;
  }
  f = (char *) dir;
  for(; i < FATSECTOR + DIRSIZE; i++) {
    bl_write(i, f);
    f += SECTORSIZE;
  }
}

void read_fat() {
  char *f = (char* ) fat;

  int i;
  for(i = 0; i < FATSECTOR; i++) {
    bl_read(i, f);
    f += SECTORSIZE;
  }

  f = (char *) dir;
  for(; i < FATSECTOR + DIRSIZE; i++) {
    bl_read(i, f);
    f += SECTORSIZE;
  }
}

int fs_init() {

  read_fat();

  for(int j = 0; j < 32; j++) {
    if (fat[j] != 3)
      formatado = 0;
  }

  if (!formatado) {
    printf("Nao formatado em FAT\n");
  }

  return 1;
}

int fs_format() {

  for(int i = 0; i < bl_size() / 8; i++) {
    if(i < 32)
      fat[i] = 3;

    else if(i == 32)
      fat[i] = 4;

    else
      fat[i] = 1;
  }

  formatado = 1;

  for (int i = 0; i < 128; i++)
      dir[i].used = 0;

  write_fat();

  return 1;
}

int fs_free() {
  int size = 0;
  for(int i = 0; i < bl_size() / 8; i++)
    if (fat[i] == 1)
      size += CLUSTERSIZE;

  return size;
}

int fs_list(char *buffer, int size) {
  buffer[0] = '\0'; // zera o buffer pra escrita
  int j = 0;

  if (!formatado){
    printf("O disco precisa ser formatado prineiro! (usar comando format)\n");
    return 0;
  }

  for(int i = 0; i < 128; i++)
    if (dir[i].used == 1){
      sprintf(buffer+j, "%s\t\t%d\n", dir[i].name, dir[i].size);
      j = strlen(buffer);
    }

  return 1;
}

int fs_create(char* file_name) {

  if (!formatado){
    printf("O disco precisa ser formatado prineiro! (usar comando format)\n");
    return 0;
  }

  for(int i = 0; i < 128; i++) {
    if(dir[i].used == 1 && !strcmp(file_name, dir[i].name)) {
      printf("O arquivo ja existe!\n");
      return 0;
    }
  }

  int j;
  for(int i = 0; i < 128; i++) {
    if(dir[i].used == 0) {
      dir[i].used = 1;
      strcpy(dir[i].name, file_name);
      for(j = 0; j < bl_size() / 8; j++) {
        if (fat[j] == 1) {
          break;
        }
      }
      dir[i].first_block = j;
      dir[i].size = 0;
      fat[j] = 2;
      break;
    }
  }

  write_fat();

  return 1;
}

int fs_remove(char *file_name) {

  if (!formatado){
    printf("O disco precisa ser formatado prineiro! (usar comando format)\n");
    return 0;
  }

  int flag = 0;
  for(int i = 0; i < 128; i++) {
    if (!strcmp(file_name, dir[i].name) && dir[i].used == 1) {
        flag = 1;
        dir[i].used = 0;
        int j;
        for (j = dir[i].first_block; fat[j] != 2; j++)
          fat[j] = 1;
        fat[j] = 1;
        break;
    }
  }

  if (!flag) {
    printf("O arquivo nao existe!\n");
    return 0;
  }

  write_fat();

  return 1;
}

void clean_buffer(char *buffer) {

  for (int i = 0; i < 4096; i++) {
    buffer[i] = 0;
  }
}

int fs_open(char *file_name, int mode) {

  if (mode == FS_R) {
    for(int i = 0; i < 128; i++)
      if( !(strcmp(dir[i].name, file_name)) && dir[i].used == 1) {
        file_save[i].posbuffer = 0;
        file_read[i].posbuffer = 0;
        file_save[i].nbytes = 0;
        file_read[i].nbytes = 0;
        file_save[i].blocoatual = dir[i].first_block;
        file_read[i].blocoatual = dir[i].first_block;
        file_save[i].aberto = -1;
        file_read[i].aberto = -1;
        file_read[i].sector = dir[i].first_block * 8;
        return i;
      }

  return -1;
  }

  if (mode == FS_W) {
    for(int i = 0; i < 128; i++)
      if(!(strcmp(dir[i].name, file_name))) {
        fs_remove(file_name);
        fs_create(file_name);
        file_save[i].posbuffer = 0;
        file_read[i].posbuffer = 0;
        file_save[i].nbytes = 0;
        file_read[i].nbytes = 0;
        file_read[i].sector = dir[i].first_block * 8;
        file_save[i].blocoatual = dir[i].first_block;
        file_read[i].blocoatual = dir[i].first_block;
        file_save[i].aberto = 1;
        file_read[i].aberto = 1;
        return i;
      }

    for(int i = 0; i < 128; i++)
      if(dir[i].used == 0) {
        fs_create(file_name);
        file_save[i].posbuffer = 0;
        file_read[i].posbuffer = 0;
        file_save[i].nbytes = 0;
        file_read[i].nbytes = 0;
        file_read[i].sector = dir[i].first_block * 8;
        file_save[i].blocoatual = dir[i].first_block;
        file_read[i].blocoatual = dir[i].first_block;
        file_save[i].aberto = 1;
        file_read[i].aberto = 1;
        return i;
      }
  }

  return -1;
}

int fs_close(int file)  {
  for(int i = 0; i < 128; i++) {
    file_save[i].aberto = 0;
    file_read[i].aberto = 0;
  }
  return 0;
}

void save_buffer(char *buffer, int file) {

  char *t = buffer;
  file_save[file].sector = file_save[file].blocoatual * 8;

  for (int i = file_save[file].sector; i < file_save[file].sector + 8; i++) {
    bl_write(i, t);

    t += SECTORSIZE;
  }

  file_save[file].sector += 8;
}

void read_buffer(char *buffer, int file) {
  char *t = buffer;

  for (int i = file_read[file].sector; i < file_read[file].sector + 8; i++) {
    bl_read(i, t);

    t += SECTORSIZE;
  }
  file_read[file].sector += 8;
}

int fs_write(char *buffer, int size, int file) {

  if (file_save[file].aberto != 1 && dir[file].used == 1)
    return -1;


  int j;
  for (j = 0; j < size; j++, file_save[file].posbuffer++) {
    if (file_save[file].posbuffer >= 4096) {


      file_save[file].posbuffer = 0;

      int i = file_save[file].blocoatual;
      for(; fat[i] != 1; i++);

      save_buffer(tempbuffer, file);
      clean_buffer(tempbuffer);
      fat[file_save[file].blocoatual] = i;
      fat[i] = 2;

      file_save[file].blocoatual = i;
      write_fat();
    }

    tempbuffer[file_save[file].posbuffer] = buffer[j];
    file_save[file].nbytes++;

    if (size != 10) {
      if (file_save[file].posbuffer > 0) {
        char *t = tempbuffer;

        file_save[file].sector = file_save[file].blocoatual * 8;
        for (int i = file_save[file].sector; i < file_save[file].sector + 8; i++) {
          bl_write(i, t);

          t += SECTORSIZE;
        }

        file_save[file].sector += 8;
        clean_buffer(tempbuffer);
        file_save[file].posbuffer = 0;

        write_fat();
      }
    }
  }
  dir[file].size += j;
  return j;
}

int fs_read(char *buffer, int size, int file) {

    if (file_save[file].aberto != -1 && dir[file].used == 0)
      return -1;

    read_fat();
    int j;
    for (j = 0; j < size;j++, file_read[file].posbuffer++) {
      file_read[file].nbytes++;
      if (file_read[file].posbuffer == 4096) file_read[file].posbuffer = 0;
      if (file_read[file].posbuffer == 0) {
        read_buffer(tempbuffer, file);
      }

      if (size == 10) {
        buffer[j] = tempbuffer[file_read[file].posbuffer];
      }
      if (file_read[file].nbytes > dir[file].size) {

        return 0;
      }
    }
    return j;
}
