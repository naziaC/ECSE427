/* Nazia Chowdhury | 261055046 | ECSE 427 | Assignment 3 */

#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Constants
#define BLOCK_SIZE 1024             // In bytes
#define BLOCK_NUMBER 4096           // Amount of blocks
#define FREEBITMAP_BLOCKS 16        // Free bitmap takes 16 blocks
#define MAX_FILE_SIZE 274432        // From 12B + B^2/d -> in bytes
#define MAX_FILE_NAME 16            // Max file length - 15 + 1 (the null terminator)
#define INODE_BLOCK_NUMBER 7        // Inode takes 7 blocks
#define DIRECTORY_BLOCK_NUMBER 3    // Directory takes 3 blocks
#define INODE_DIR_ENTRY_LENGTH 126  // Number of entries in both inode and directory table
#define MAX_FILE_DESCRIPTOR 16      // Max amount of file open
#define FILENAME_FOR_DISK "sfs.disk"// Name for the disk
#define MAGIC 0xACBD0005            // Magic number found in handout
#define MAX_DIRECT_PTR 12           // Number of direct pointers

typedef struct {
    int magic;
    int block_size;         // 1024
    int file_system_size;   // # of blocks in the file system
    int inode_table_length; // # of blocks to contain all i-nodes
    int root_directory;     // pointer to the i-node for the root directory
} superBlock;

typedef struct {
    int size; // in bytes
    int direct_ptrs[MAX_DIRECT_PTR]; // array for direct pointers
    int indirect_ptr; // indirect pointer
} inode; // has a size of 56 bytes

typedef struct {
    int used; // used directory entry or not
    int inode_number; // inode number
    char filename[MAX_FILE_NAME]; // file
} directoryEntry; // has a size of 24 bytes

typedef struct {
    int inode_number; // inode number 
    int rw_pointer; // rw pointer
} fileDescriptorEntry;

// Global variables - cache
superBlock super_block;
fileDescriptorEntry file_descriptor_table[MAX_FILE_DESCRIPTOR];
directoryEntry directory_table[INODE_DIR_ENTRY_LENGTH];
inode inode_table[INODE_DIR_ENTRY_LENGTH];
int free_bitmap_array[BLOCK_NUMBER];
// For sfs_getnextfilename()
int current_directory_filename;

// ------- Helper functions for free bitmap ----------------

// Returns the index of the first free block
int allocate_block_FBM() {
    for (int i = 0; i < BLOCK_NUMBER; i++) {
        // if 1 then free to use, 0 if occupied
        if (free_bitmap_array[i] == 1) {
            free_bitmap_array[i] = 0;
            return i;
        }
    }
    return -1;
}

// Deallocates the block (frees)
void deallocate_block_FBM(int index_to_free) {
    free_bitmap_array[index_to_free] = 1;
}
// ---------------------------------------------------------

void mksfs(int fresh) {
    if(fresh){
        // Create new file system
        init_fresh_disk(FILENAME_FOR_DISK, BLOCK_SIZE, BLOCK_NUMBER);

        // Initialize the superblock
        super_block.magic = MAGIC;
        super_block.block_size = BLOCK_SIZE;
        super_block.file_system_size = BLOCK_NUMBER;
        super_block.inode_table_length = INODE_BLOCK_NUMBER;
        super_block.root_directory = 0;
        
        // Initialize the free bitmap
        for (int i = 0; i < BLOCK_NUMBER; i++) {
            free_bitmap_array[i] = 1; // 1 means free to use, 0 means used

            // occupied if superblock, inode table, dir table, free bitmap
            if (i == 0) free_bitmap_array[i] = 0;
            if (i >= 1 && i <= INODE_BLOCK_NUMBER) free_bitmap_array[i] = 0;
            if (i >= INODE_BLOCK_NUMBER + 1 && i <= INODE_BLOCK_NUMBER + DIRECTORY_BLOCK_NUMBER) free_bitmap_array[i] = 0;
            if (i >= BLOCK_NUMBER - FREEBITMAP_BLOCKS - 1) free_bitmap_array[i] = 0;
        }

        // Initialize the inode table
        for (int i = 0; i < INODE_DIR_ENTRY_LENGTH; i++) {
            inode_table[i].size = -1;
            inode_table[i].indirect_ptr = -1;
            for (int j = 0; j < MAX_DIRECT_PTR; j++) {
                inode_table[i].direct_ptrs[j] = -1;
            }
        }

        // Initialize the directory table
        for (int i = 1; i < INODE_DIR_ENTRY_LENGTH; i++) {
            directory_table[i].used = 0; // 0 means not used, 1 means used
            directory_table[i].inode_number = -1;
            strcpy(directory_table[i].filename, "");
        }

        // Initialize the file descriptor table
        for (int i = 0; i < MAX_FILE_DESCRIPTOR; i++) {
            file_descriptor_table[i].inode_number = -1;
            file_descriptor_table[i].rw_pointer = -1;
        }

        // Add an entry in directory table and inode table for root directory
        directory_table[0].used = 1;
        directory_table[0].inode_number = 0;
        strcpy(directory_table[0].filename, "");
        inode_table[0].size = 0;

        // Initialize pointer used for the sfs_getnextfilename()
        current_directory_filename = 0;

        // Write everything to disk (superblock, inode table, dir table, free bitmap)
        if (write_blocks(0, 1, &super_block) < 0) printf("write_blocks(super_block) in mksfs() did not work \n");
        if (write_blocks(1, INODE_BLOCK_NUMBER, &inode_table) < 0) printf("write_blocks(inode_table) in mksfs() did not work \n");
        if (write_blocks(INODE_BLOCK_NUMBER + 1, DIRECTORY_BLOCK_NUMBER, &directory_table) < 0) printf("write_blocks(directory_table) in mksfs() did not work \n");
        if (write_blocks(BLOCK_NUMBER - FREEBITMAP_BLOCKS - 1, FREEBITMAP_BLOCKS, &free_bitmap_array) < 0) printf("write_blocks(free_bitmap_array) in mksfs() did not work \n");
    } else {
        // Load existing file system
        init_disk(FILENAME_FOR_DISK, BLOCK_SIZE, BLOCK_NUMBER);

        // Initialize pointer used for the sfs_getnextfilename()
        current_directory_filename = 0;

        // Load everything from disk (superblock, inode table, dir table, free bitmap)
        if (read_blocks(0, 1, &super_block) < 0) printf("write_blocks(super_block) in mksfs() did not work \n");
        if (read_blocks(1, INODE_BLOCK_NUMBER, &inode_table) < 0) printf("write_blocks(inode_table) in mksfs() did not work \n");
        if (read_blocks(INODE_BLOCK_NUMBER + 1, DIRECTORY_BLOCK_NUMBER, &directory_table) < 0) printf("write_blocks(directory_table) in mksfs() did not work \n");
        if (read_blocks(BLOCK_NUMBER - FREEBITMAP_BLOCKS - 1, FREEBITMAP_BLOCKS, &free_bitmap_array) < 0) printf("write_blocks(free_bitmap_array) in mksfs() did not work \n");
    }
}

int sfs_fopen(char *name) {
    // Validate the name
    if (strlen(name) >= MAX_FILE_NAME) {
        printf("File name is longer than allowed - max 15 characters \n"); // + 1 character for null terminator
		return -1;
	}

    // Check if the file exists in the directory table
    for (int i = 0; i < INODE_DIR_ENTRY_LENGTH; i++) {
        if (strcmp(directory_table[i].filename, name) == 0) {
            // Check if the file is already opened in the file descriptor table and return the index if it is
            for (int j = 0; j < MAX_FILE_DESCRIPTOR; j++) {
                if (file_descriptor_table[j].inode_number == directory_table[i].inode_number) {
                    // Append mode
                    file_descriptor_table[j].rw_pointer = inode_table[directory_table[i].inode_number].size;
                    return j;
                }
            }
            // File exists but is not present in the file descriptor table so we create a new entry in the file descriptor table
            for (int j = 0; j < MAX_FILE_DESCRIPTOR; j++) {
                if (file_descriptor_table[j].inode_number == -1) {
                    file_descriptor_table[j].inode_number = directory_table[i].inode_number;
                    file_descriptor_table[j].rw_pointer = inode_table[directory_table[i].inode_number].size;;
                    return j;
                }
            }
        }
    }

    // File does not exist, so we create a new file...

    // Check if there is space in the directory table
    int dirEntry = -1;
    for (int i = 0; i < INODE_DIR_ENTRY_LENGTH; i++) {
        if (directory_table[i].inode_number == -1) {
            dirEntry = i;
            break;
        }
    }

    if (dirEntry == -1) {
        printf("No available directory entry found - remove some files?\n");
        return -1;
    }
    
    // check if there is space in the inode table
    int inodeEntry = -1;
    for (int i = 0; i < INODE_DIR_ENTRY_LENGTH; i++) {
        if (inode_table[i].size == -1) {
            inodeEntry = i;
            break;
        }
    }

    if (inodeEntry == -1) {
        printf("No available inode found - remove some files?\n");
        return -1;
    }

    // Create inode and directory entry for the new file
    inode_table[inodeEntry].size = 0;
    directory_table[dirEntry].inode_number = inodeEntry;
    strcpy(directory_table[dirEntry].filename, name);
    directory_table[dirEntry].used = 1;

    // Write to disk that a new inode and directory entry has been created - maybe not necessary?
    if (write_blocks(1, INODE_BLOCK_NUMBER, &inode_table) < 0) printf("write_blocks(inode_table) in sfs_fopen() did not work\n");
    if (write_blocks(INODE_BLOCK_NUMBER + 1, DIRECTORY_BLOCK_NUMBER, &directory_table) < 0) printf("write_blocks(directory_table) in sfs_fopen() did not work\n");

    // Add to file descriptor table (open the file) if there is space
    for (int i = 0; i < MAX_FILE_DESCRIPTOR; i++) {
        if (file_descriptor_table[i].inode_number == -1) {
            file_descriptor_table[i].inode_number = inodeEntry;
            file_descriptor_table[i].rw_pointer = 0;
            return i;
        }
    }
    printf("File created but no available file descriptor found - please close some files and try again\n");
    return -1;
}

int sfs_fclose(int fileID) {
    // Check if file exists and close it if it does, else give an error
	if (file_descriptor_table[fileID].inode_number == -1) { 
        printf("Error closing file: No file associated with that fileID\n");
        return -1; 
    } else {
		file_descriptor_table[fileID].inode_number = -1;
		file_descriptor_table[fileID].rw_pointer = -1;
		return 0;	
	}
}

int sfs_fwrite(int fileID, const char *buf, int length) {

    if(file_descriptor_table[fileID].inode_number == -1){
        printf("Can't write to a file that's not opened!\n");
        return -1;
    }

    // Make sure that we're not writing too much (truncate if needed)
    int rw_pointer = file_descriptor_table[fileID].rw_pointer;
    if(length + rw_pointer > MAX_FILE_SIZE) length = MAX_FILE_SIZE - rw_pointer;

    int indirect_pointers_array[BLOCK_SIZE/sizeof(int)]; // Array for indirect pointer
    int first_write_block = rw_pointer / BLOCK_SIZE; // First block that will be written into
    int last_write_block = (rw_pointer + length) / BLOCK_SIZE; // Last block that will be written into
    int amt_written = 0; // For return, keeps track of how much is written

    // Get current inode
    int inode_number = file_descriptor_table[fileID].inode_number;
    inode *inode = &inode_table[inode_number];

    // Create a temp block buffer to read what is inside the block
    void* temp_block = (void*) malloc(BLOCK_SIZE);
    // Boolean for indirect pointer
    bool bool_for_indirect_pointers_array = false;
    
    for (int i = first_write_block; i <= last_write_block; i++) {
        // Calculate the offset for what's written in the block, the amount left in the block, and how much bytes we can write in the current block
        int offset = (i == first_write_block) ? rw_pointer % BLOCK_SIZE : 0;
        int block_size = (i == last_write_block) ? (rw_pointer + length) % BLOCK_SIZE : BLOCK_SIZE;
        int bytes_written = block_size - offset;

        if (i < MAX_DIRECT_PTR) {
            // Allocate block if neccesary
            if(inode->direct_ptrs[i] == -1) {
                int allocatedBlock = allocate_block_FBM();
                if (allocatedBlock < 0) {
                    printf("Error allocating blocks - not enough space, sorry!\n");
                    return -1;
                }
                inode->direct_ptrs[i] = allocatedBlock;
            }

            // Read the block and write into it
            read_blocks(inode->direct_ptrs[i], 1, temp_block);

            memcpy(temp_block + offset, buf + amt_written, bytes_written);

            write_blocks(inode->direct_ptrs[i], 1, temp_block);
        } else {
            // It falls into the indirect pointers
            bool_for_indirect_pointers_array = true;
            if (inode->indirect_ptr == -1) {
                // Allocate block if necessary
                if(inode->indirect_ptr == -1) {
                    int allocatedBlock = allocate_block_FBM();
                    if (allocatedBlock < 0) {
                        printf("Error allocating blocks - not enough space, sorry!\n");
                        return -1;
                    }
                    inode->indirect_ptr = allocatedBlock;
                }

                for (int i = 0; i < BLOCK_SIZE/sizeof(int); i++) {
                    indirect_pointers_array[i] = -1;
                }

                // Write into the block
                memcpy(temp_block, indirect_pointers_array, BLOCK_SIZE);
                write_blocks(inode->indirect_ptr, 1, temp_block);
            }

            // Read the indirect pointer block 
            read_blocks(inode->indirect_ptr, 1, temp_block);

            // Copy content of the indirect block into the array
            memcpy(indirect_pointers_array, temp_block, BLOCK_SIZE);

            // Check if the indirect block at (i - 12) is uninitialized
            if (indirect_pointers_array[i - 12] == -1) { 
                int allocatedBlock = allocate_block_FBM();
                if (allocatedBlock < 0) {
                    printf("Error allocating blocks - not enough space, sorry!\n");
                    return -1;
                }
                indirect_pointers_array[i - 12] = allocatedBlock;
            }

            // Read the block and write into it
            read_blocks(indirect_pointers_array[i - 12], 1, temp_block);
            memcpy(temp_block + offset, buf + amt_written, bytes_written);
            write_blocks(indirect_pointers_array[i - 12], 1, temp_block);
        }

        amt_written += bytes_written;
        
    }

    // If we wrote into the indirect pointer, then we need to update accordingly
    if (bool_for_indirect_pointers_array) {
        memcpy(temp_block, indirect_pointers_array, BLOCK_SIZE);
        write_blocks(inode->indirect_ptr, 1, temp_block);
    }

    // Modify the rw_pointer and file size in the file descriptor table and the inode table
    fileDescriptorEntry* file_descriptor_entry = &file_descriptor_table[fileID];
    file_descriptor_entry->rw_pointer += length;
    if (file_descriptor_entry->rw_pointer > inode->size) inode->size = file_descriptor_entry->rw_pointer;

    // Update free bitmap and inode table - FLUSH
    if (write_blocks(BLOCK_NUMBER - FREEBITMAP_BLOCKS -1, FREEBITMAP_BLOCKS, &free_bitmap_array) > 0 && write_blocks(1, INODE_BLOCK_NUMBER, &inode_table) > 0) {
        free(temp_block);
        return amt_written;
    } else {
        printf("Error: cannot write block\n");
        free(temp_block);
        return amt_written;
    }
}

int sfs_fread(int fileID, char *buf, int length) {

    if(file_descriptor_table[fileID].inode_number == -1){
        printf("Can't write to a file that's not opened!\n");
        return -1;
    }

    // Get current inode
    int inode_number = file_descriptor_table[fileID].inode_number;
    inode *inode = &inode_table[inode_number];

    // If we're reading more than the file size, change length to file size
    int rw_pointer = file_descriptor_table[fileID].rw_pointer;
    if (rw_pointer + length > inode->size) length = inode->size;

    int first_read_block = rw_pointer / BLOCK_SIZE; // First block that will be read
    int last_read_block = (rw_pointer + length) / BLOCK_SIZE; // Last block that will be written into
    int amt_written = 0; // For return, keeps track of how much is written

    // Create a temp block buffer to read what is inside the block
    void* temp_block = (void*) malloc(BLOCK_SIZE);

    for (int i = first_read_block; i <= last_read_block; i++) {
        // Calculate the offset for what's read in the block, the amount left in the block to read, and how much bytes we can read in the current block
        int offset = (i == first_read_block) ? rw_pointer % BLOCK_SIZE : 0;
        int block_size = (i == last_read_block) ? (rw_pointer + length) % BLOCK_SIZE : BLOCK_SIZE;
        int bytes_read = block_size - offset;

        // If inside direct or indirect
        if (i < MAX_DIRECT_PTR) {

            read_blocks(inode->direct_ptrs[i], 1, temp_block);
            memcpy(buf + amt_written, temp_block + offset, bytes_read);

        } else {
            // It falls into the indirect pointers
            int indirect_pointers_array[BLOCK_SIZE/sizeof(int)];

            read_blocks(inode->indirect_ptr, 1, temp_block);
            memcpy(indirect_pointers_array, temp_block, BLOCK_SIZE);

            read_blocks(indirect_pointers_array[i - 12], 1, temp_block);
            memcpy(buf + amt_written, temp_block + offset, bytes_read);
        }

        amt_written += bytes_read;
    }

    // Modify the rw_pointer in the file descriptor table
    file_descriptor_table[fileID].rw_pointer += length;

    free(temp_block);
    
    return amt_written;
}

int sfs_fseek(int fileID, int offset) {
    // Check if file is open first (can't seek if file is not open)
    if (file_descriptor_table[fileID].inode_number == -1) {
        printf("File is not open. Please open before using sfs_fseek()!\n");
        return -1;
    } else {
        if (offset >= 0) {
            // Set the read/write pointer based on the specified offset
            // If offset is greater than the file size, set the rw pointer to the end of the file
            // else set it to the offset
            file_descriptor_table[fileID].rw_pointer = (offset > inode_table[fileID].size) ? inode_table[fileID].size : offset;
        } else {
            // Set the rw pointer to the beginning of the file if offset is negative
            file_descriptor_table[fileID].rw_pointer = 0;
        }
        return 0;
    }
}

int sfs_remove(char *file) {
    int inode_number = -1;

    // Get the inode number of the file
    for (int i = 0; i < INODE_DIR_ENTRY_LENGTH; i++) {
        if (strcmp(file, directory_table[i].filename) != 0) continue;
        inode_number = directory_table[i].inode_number;
    }

    // If file exists remove else error
    if (inode_number != -1) {

        // Remove file from directory tabel table
        for (int i = 0; i < INODE_DIR_ENTRY_LENGTH; i++) {
            if (strcmp(directory_table[i].filename, file) == 0) {
                directory_table[i].inode_number = -1;
                strcpy(directory_table[i].filename, "");
                break;
            }
        }

        // Remove file from file descriptor table
        for (int i = 0; i < MAX_FILE_DESCRIPTOR; i++) {
            if (file_descriptor_table[i].inode_number == inode_number) {
                file_descriptor_table[i].inode_number = -1;
                file_descriptor_table[i].rw_pointer = -1;
            }
        }

        // Remove file from inode table
        inode *inode = &inode_table[inode_number];

        void *temp_block = (void *) malloc(BLOCK_SIZE);
        int last_block = inode->size/BLOCK_SIZE;

        for (int i = 0; i <= last_block; i++) {
            memset(temp_block, 0, BLOCK_SIZE);
            // If direct pointer or indirect pointer
            if (i < 12) {
                // Clear the block and deallocate the block from free bitmap array
                write_blocks(inode->direct_ptrs[i], 1, temp_block);
                deallocate_block_FBM(inode->direct_ptrs[i]);
                inode->direct_ptrs[i] = -1;
            } else {
                // Clear the block and deallocate the block from free bitmap array
                int indirect_pointers_array[BLOCK_SIZE/sizeof(int)];
                read_blocks(inode->indirect_ptr, 1, temp_block);
                memcpy(indirect_pointers_array, temp_block, BLOCK_SIZE);
                memset(temp_block, 0, BLOCK_SIZE);
                write_blocks(indirect_pointers_array[i - 12], 1, temp_block);
                deallocate_block_FBM(indirect_pointers_array[i - 12]);
                indirect_pointers_array[i - 12] = -1;
            }
        }
        // Free up the block
        deallocate_block_FBM(inode->indirect_ptr);
        inode->indirect_ptr = -1;

        // Write these changes onto the disk - FLUSH
        if (write_blocks(INODE_BLOCK_NUMBER + 1, DIRECTORY_BLOCK_NUMBER, &directory_table) > 0 &&
            write_blocks(BLOCK_NUMBER - FREEBITMAP_BLOCKS - 1, FREEBITMAP_BLOCKS, &free_bitmap_array) > 0 &&  
            write_blocks(1, INODE_BLOCK_NUMBER, &inode_table) > 0) {
            free(temp_block);
            return 1;
        } else {
            free(temp_block);
            printf("Error when writing to blocks!\n");
            return 0;
        }
    } 

    printf("File not found!\n");
    return -1;
}

// -------------- Test 2 ------------------
int sfs_getnextfilename(char *fname) {
    // Check validity of the current directory filename (the index)
    if (current_directory_filename >= INODE_DIR_ENTRY_LENGTH) {
        return -1;
    }

    // Check if the current directory filename is valid
    if (directory_table[current_directory_filename].inode_number == -1) {
        return -1;
    }

    // Copy the filename in the directory table to fname
    strcpy(fname, directory_table[current_directory_filename].filename);

    current_directory_filename++;
    return 0;
}

int sfs_getfilesize(const char *path) {
    // Find the file and get the size -> return it
    for (int i = 0; i < INODE_DIR_ENTRY_LENGTH; i++) {
        if ((directory_table[i].inode_number != -1) && (strcmp(directory_table[i].filename, path) == 0)) {
            return inode_table[directory_table[i].inode_number].size;
        } else {
            continue;
        }
    }
    return -1;
}