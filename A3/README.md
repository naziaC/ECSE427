Assignment 3 - Simple File System - Nazia Chowdhury - 261055046

Important Test Parameters:
- MAX_FNAME_LENGTH = 15
- MAX_FD 16 (this is the amount of files that can be opened at once)
- MAX_BYTES 15000
- MIN_BYTES 5000

Important Implementation Details:
- For the makefile, I do not have the following files; sfs_dir.c and sfs_inode.c, so it looks like this;
        SOURCES= disk_emu.c sfs_api.c sfs_test0.c sfs_api.h
        SOURCES= disk_emu.c sfs_api.c sfs_test1.c sfs_api.h
        SOURCES= disk_emu.c sfs_api.c sfs_test2.c sfs_api.h

- For the makefile, I am using a MAC and could not use the fuse wrapper so this is how my flags look like:
        CFLAGS = -c -g -ansi -pedantic -Wall -std=gnu99 
        LDFLAGS = 

- When running make, please ignore the warnings - the program works as expected despite it

