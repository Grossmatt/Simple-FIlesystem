// Matthew Grossweiler 1001626716


// The MIT License (MIT)
//
// Copyright (c) 2016, 2017 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES 128
#define MAX_FILE_SIZE 10240000

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

FILE * di = NULL;
FILE * fd;


uint8_t blocks[NUM_BLOCKS][BLOCK_SIZE];

struct Inode // Defining our inode structure
{

 time_t time;
 uint8_t valid;
 uint8_t attribute;
 uint32_t size;
 int blocks[1250];

};

struct Directory_Entry  // Defining our directory structure
{

 uint8_t valid;
 uint32_t inode;
 char filename[255];

};

struct Directory_Entry * dir;
struct Inode * inodes;
uint8_t * freeblockslist;
uint8_t * freeinodelist;

void initializedirectory() // Setting all directory entries to 0 for invalid
{
    int i;
    for (i = 0; i < NUM_FILES; i ++)
    {
        dir[i].valid = 0;
        dir[i].inode = -1;
        memset(dir[i].filename,0,255);
    }
}

void initializefreeinodelist() // Setting all inodes free in free inodes map
{
    int i;
    for (i = 0; i < NUM_FILES; i ++)
    {
        freeinodelist[i] = 1;
    }
}

void initializefreeblockslist()  /// Setting all blocks free in free blocks map
{
    int i;
    for (i = 10; i < NUM_BLOCKS; i ++)
    {
        freeblockslist[i] = 1;
    }
}

void initializeinodes() // Initializing all inodes to 0 for invalid. Also setting all its blocks to -1 for empty.
{
    int i;
    for (i = 0; i < NUM_FILES; i ++)
    {
        inodes[i].time = 0;
        inodes[i].valid = 0;
        inodes[i].size = 0;
        inodes[i].attribute = 0;
        int j;
        for (j = 0; j < 1250; j++)
        {
            inodes[i].blocks[j] = -1;
        }
    }
}

int df() // Iterates through all blocks and adds together how many of them are free using the free block map
{
    int i;
    int free_space = 0;
    for (i = 10; i < NUM_BLOCKS; i++)
    {
        if (freeblockslist[i] == 1)
        {
            free_space = free_space + BLOCK_SIZE;
        }
    }
    return free_space;
}

void createfs(char * filename) // Opens a file. Sets all data to 0 in our blocks array. Initializeds all the structures. Saves the meta data into the file.
{
    di = fopen(filename, "w");
    memset(&blocks[0], 0, NUM_BLOCKS * BLOCK_SIZE);
    initializedirectory();
    initializefreeinodelist();
    initializefreeblockslist();
    initializeinodes();
    fwrite(&blocks[0], NUM_BLOCKS, BLOCK_SIZE, di);
    fclose(di);
}

void open(char * file_image_name) // Opens the file and moves its data into the blocks array.
{
    di = fopen(file_image_name, "r");
    if (di == NULL)
    {
       printf("File not found.\n");
       return -1;
    }
    fread(&blocks[0], NUM_BLOCKS, BLOCK_SIZE, di);
    fclose(di);

}

void closeme( char * file_image_name ) // Opens the target file and moves the data in the blocks array into the file. Uses ret to check if file exists.
                                       // Requires a filename to use close.
{
    struct stat buf;
    int ret = stat(file_image_name, &buf);
    if(ret == -1)
    {
      printf("File does not exist.\n");
      return -1;
    }

    di = fopen(file_image_name, "w");
    if (di == NULL)
    {
      printf("No file open.\n");
      return -1;
    }
    fwrite(&blocks[0], NUM_BLOCKS, BLOCK_SIZE, di);
    fclose(di);
}


void list(char * att) // Lists files in the blocks array.
{
    int i;
    int inode_idx;
    char * string_time;
    int flag = 0;


    if(att != NULL && strcmp(att, "-h") == 0) // Prints all files including hidden files.
    {
      for (i = 0; i < NUM_FILES; i ++)
      {
        inode_idx = dir[i].inode;
        if(dir[i].valid == 1)
        {
          flag = 1;
          string_time = ctime(&inodes[inode_idx].time);
          printf("\nFilename: %42s\nSize:%10d\nTime of last status change: %s\n", dir[i].filename, inodes[inode_idx].size, string_time);
        }
      }
    }
    else  // Prints all files excluding hidden files.
    {
      for (i = 0; i < NUM_FILES; i++)
      {
        inode_idx = dir[i].inode;
        if((dir[i].valid == 1) && (inodes[inode_idx].attribute != 1) && (inodes[inode_idx].attribute != 3))
        {
          flag = 1;
          string_time = ctime(&inodes[inode_idx].time);
          printf("\nFilename: %42s\nSize:%10d\nTime of last status change: %s\n", dir[i].filename, inodes[inode_idx].size, string_time);
        }
      }
    }


    if(flag == 0)
    {
      printf("No files found.\n");
    }

}

int finddirectoryindex (char * filename) // Searches the directory for a file. If it doesnt exist then a new free directory is found and that index is passed instead.
{
    int i;
    int ret = -1;
    for (i = 0; i < NUM_FILES; i ++)
    {
        if (strcmp(filename, dir[i].filename) == 0)
        {
            return i;
        }
    }
    for (i = 0; i < NUM_FILES; i ++)
    {
        if (dir[i].valid == 0)
        {
            dir[i].valid = 1;
            return i;
        }
    }
    return ret;
}

int findfreeinode() // Searches through all of the inodes to find a free one then returns the index.
{
    int i;
    int ret = -1;
    for (i = 0; i < NUM_FILES; i ++)
    {
        if (inodes[i].valid == 0)
        {
            inodes[i].valid = 1;
            return i;
        }
    }
    return ret;
}

int findfreeblock() // Searches through the free block map to find free blocks and sets them as used.
{
    int i;
    int ret = -1;
    for (i = 10; i < NUM_BLOCKS; i ++)
    {
        if (freeblockslist[i] == 1)
        {
            freeblockslist[i] = 0;
            return i;
        }
    }
    return ret;
}

void put(char * filename)
{
    struct stat buf;
    int ret;
    int block_idx = 0;

    ret = stat(filename, &buf); // Checking for file existance.
    if (ret == -1)
    {
        printf("File doesn't exist.\n");
        return -1;
    }

    int size = buf.st_size; // Getting the size of the file.
    time_t time = buf.st_ctime; // Getting the access time.

    if (size > MAX_FILE_SIZE) // Checking if the file is too big to fit in an inode.
    {
        printf("File size too big.\n");
        return -1;
    }

    if (size > df()) // Checking if there is enough space left in the blocks array.
    {
        printf("File exceeds remaining disk space.\n");
        return -1;
    }

    int directoryindex = finddirectoryindex(filename); // Finding the index where 'filename' exists in the directory.
    int inodeindex = findfreeinode(); // Find a free inode for the file.
    int freeblockindex;

    if (sizeof(filename) > 255) // Checks if the filename is too long.
    {
       printf("Filename too long.\n");
       return -1;
    }

    dir[directoryindex].valid = 1; // Sets the directory for this file to valid.
    strncpy(dir[directoryindex].filename, filename, strlen(filename)); // Copies the name into the directory struct.
    dir[directoryindex].inode = inodeindex; // Assigns an inode to that file in the directory.

    inodes[inodeindex].valid = 1; // Setting the values for the inode struct.
    inodes[inodeindex].size = size;
    inodes[inodeindex].time = time;
    freeinodelist[inodeindex] = 0;

    fd = fopen (filename, "r"); // Opening the file to grab the info.
    printf("Reading %d bytes from %s\n", (int)buf.st_size, filename);

    int offset = 0;

    while (size > 0)
    {
      freeblockindex = findfreeblock();
      fseek (fd, offset, SEEK_SET);

      int bytes = fread (&blocks[freeblockindex], BLOCK_SIZE, 1, fd); // Reading one block of data and saving it into blocks array.

      if (bytes == 0 && !feof (fd))
      {
        printf("An error occured reading from the input file.\n");
        return -1;
      }

      clearerr (fd);

      freeblockslist[freeblockindex] = 0;
      size -= BLOCK_SIZE;
      offset += BLOCK_SIZE;
      inodes[inodeindex].blocks[block_idx] = freeblockindex; // Mapping the inode block array to the free blocks map.
      block_idx = block_idx + 1;
    }

    fclose (fd);
}

void get (char * filename, char * newfilename)
{
   int i, dir_idx, inode_idx;
   int flag = 0;
   struct stat buf;


   for (i = 0; i < NUM_FILES; i++) // Finding the index of the filename if the file exists.
   {
      if (strcmp (filename, dir[i].filename) == 0)
      {
         dir_idx = i;
         inode_idx = dir[dir_idx].inode;
         flag = 1;
      }

   }



   if (flag == 0)
   {
     printf("Could not open output file: %s\n", filename);

     perror("Opening output file returned.");
     return -1;
   }

   int offset = 0;
   int copy_size = inodes[inode_idx].size;


   if (newfilename == NULL) // If a new filename was given then a new file is created.
   {
     printf("Writing %d bytes to %s\n", copy_size, filename);
     fd = fopen(filename, "w");
   }
   else // Else overwrite the old file.
   {
     printf("Writing %d bytes to %s\n", copy_size, newfilename);
     fd = fopen(newfilename, "w");
   }

   int block_idx = 0;

   while(copy_size > 0)
   {

     int num_bytes;

     if (copy_size < BLOCK_SIZE) // Checking if number of bytes left is less than a block to only copy those bytes and not extra garbage.
     {
       num_bytes = copy_size;
     }
     else
     {
       num_bytes = BLOCK_SIZE;
     }

     fwrite (&blocks[inodes[inode_idx].blocks[block_idx++]], num_bytes, 1, fd); // Writing the data in blocks array to directory.

     copy_size -= BLOCK_SIZE;
     offset += BLOCK_SIZE;

     fseek (fd, offset, SEEK_SET);

   }


   fclose (fd);


}



void delete(char * filename)
{
   int i, j;
   int index;
   int flag = 0;
   int flag2 = 0;

   for(i = 0; i < NUM_FILES; i++)
   {
     if(strcmp(filename, dir[i].filename) == 0) // Checking if the filename exists in the blocks array.
     {
       flag = 1;
       if((inodes[dir[i].inode].attribute != 2) && (inodes[dir[i].inode].attribute != 3)) // Checking if the file is in read-only mode.
       {
         flag2 = 1;

         dir[i].valid = 0; // Resetting all of the values of the directory and inode.
         index = dir[i].inode;
         dir[i].inode = -1;
         inodes[index].valid = 0;
         freeinodelist[index] = 1;
         inodes[index].attribute = 0;
         for(j = 0; j < 1250; j++)
         {
           if(inodes[index].blocks[j] != -1)
           {
             freeblockslist[inodes[index].blocks[j]] = 1; // Freeing the inodes block array and its mapping to the free blocks map.
             inodes[index].blocks[j] = -1;
           }
         }
         return;
       }
     }
   }

   if(flag == 0)
   {
     printf("File does not exist.\n");
     return;
   }
   else if(flag2 == 0)
   {
     printf("%s is in read-only mode.\n", filename);
   }
}



void attribute (char * attrib, char * filename)
{
   int i;
   int flag = 0;
   int flag2 = 0;
   int dir_idx = 0;

   for(i = 0; i < NUM_FILES; i++)
   {
     if(strcmp(filename, dir[i].filename) == 0) // Checking if the filename exists in the directory.
     {
       flag = 1;
       dir_idx = i;
     }
   }

   if (flag == 0)
   {
     printf("File not found.\n");
     return;
   }

   /*
    Setting the attributes here.

    0 Indicates that niether flag is set.
    1 Indicates that the file is in hidden mode.
    2 Indicates that the file is in read-only mode.
    3 Indicates that the file in in both states.
   */

   if(strcmp(attrib, "+h") == 0)
   {
     if(inodes[dir[dir_idx].inode].attribute == 2)
     {
        inodes[dir[dir_idx].inode].attribute = 3;
        printf("%s is now in read-only mode and hidden.\n", filename);
     }
     else
     {
        inodes[dir[dir_idx].inode].attribute = 1;
        printf("%s is now hidden.\n", filename);
     }
   }
   else if(strcmp(attrib, "-h") == 0)
   {
     if(inodes[dir[dir_idx].inode].attribute == 3)
     {
        inodes[dir[dir_idx].inode].attribute = 2;
        printf("%s is now in read-only mode.\n", filename);
     }
     else
     {
        inodes[dir[dir_idx].inode].attribute = 0;
        printf("%s has no attribute.\n", filename);
     }
   }
   else if(strcmp(attrib, "+r") == 0)
   {
     if(inodes[dir[dir_idx].inode].attribute == 1)
     {
       inodes[dir[dir_idx].inode].attribute = 3;
       printf("%s is now in read-only mode and hidden.\n", filename);
     }
     else
     {
       inodes[dir[dir_idx].inode].attribute = 2;
       printf("%s is now in read-only mode.\n", filename);
     }
   }
   else if(strcmp(attrib, "-r") == 0)
   {
     if(inodes[dir[dir_idx].inode].attribute == 3)
     {
       inodes[dir[dir_idx].inode].attribute = 1;
       printf("%s is now hidden.\n", filename);
     }
     else
     {
       inodes[dir[dir_idx].inode].attribute = 0;
       printf("%s has no attribute.\n", filename);
     }
   }



}


int main()
{

  int i;

  dir = (struct Directory_Entry *)&blocks[0];
  freeinodelist = (uint8_t *)&blocks[7];
  freeblockslist = (uint8_t *)&blocks[8];
  inodes = (struct Inode *)&blocks[9];





  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;

    char *working_str  = strdup( cmd_str );

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    printf("If using close, please provide a Disk Name.\n");

    if (token[0] == '\0')
    {
        continue;
    }


     if ( ( strcmp("exit", token[0]) == 0) || (strcmp("quit", token[0]) == 0))
    {
      exit(0);
    }

    if (strcmp ("put", token[0]) == 0)
    {
        put(token[1]);
    }

    if (strcmp ("list", token[0]) == 0)
    {
        list(token[1]);
    }

    if (strcmp ("df", token[0]) == 0)
    {
        printf("%d bytes free.\n", df());
    }

    if (strcmp ("createfs", token[0]) == 0)
    {
        if (token[1] == NULL)
        {
           printf("No disk name stated.\n");
           continue;
        }
        createfs(token[1]);
    }

    if (strcmp ("open", token[0]) == 0)
    {
        open(token[1]);
    }

    if (strcmp ("close", token[0]) == 0)
    {
        closeme(token[1]);
    }

    if (strcmp ("del", token[0]) == 0)
    {
        delete(token[1]);
    }

    if (strcmp ("get", token[0]) == 0)
    {
       if (token[2])
       {
         get(token[1], token[2]);
       }
       else
       {
         get(token[1], NULL);
       }
    }

    if (strcmp ("attrib", token[0]) == 0)
    {
       attribute(token[1], token[2]);
    }





    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your shell functionality

   /* int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ )
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );
    }*/

    free( working_root );

  }

  return 0;
}
