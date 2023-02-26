#pragma once
#include <cstdio>

// Maximum number of files in a package
#define MPAK_MAX_FILES 1024

// Modes
#define MPAK_READ 0
#define MPAK_WRITE 1
#define MPAK_CLOSED 2

using UINT32 = unsigned int;

// Struct representing a MPak file
struct MPAK_FILE {
    UINT32 num_files;     // Number of files
    UINT32 crc_checksum;  // CRC32 checksum
    char override_dir[64];// Name of the override directory
    FILE *fpointer;       // Pointer to the MPK file
    char mpk_file[64];    // Filename of the open MPK file
    int mode;             // Opened for writing or reading?

    char files[MPAK_MAX_FILES][64];// File names
    UINT32 filetable_offset;       // Offset to the file table
    UINT32 offsets[MPAK_MAX_FILES];// File offsets
    UINT32 sizes[MPAK_MAX_FILES];  // File sizes
    UINT32 current_file_size;      // Size of the current file being open

    void init();

    int open_mpk(int open_mode, const char *file, const char *override = nullptr);

    void close_mpk();

    int add_file(const char *file);

    FILE *open_file(const char *file);

    int extract_file(const char *file, const char *path = nullptr);

    int find_file(const char *file);
};
