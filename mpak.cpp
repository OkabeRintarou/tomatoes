#include "mpak.h"
#include <cstring>
#include <string>

#define MPAK_BIG_ENDIAN 0
#define MPAK_LITTLE_ENDIAN 1

static int mpk_endian;

static inline UINT32 mpk_swap_uint32(UINT32 num) {
    return ((num << 24) | ((num << 8) & 0x00FF0000) | ((num >> 8) & 0x0000FF00) | (num >> 24));
}

static void mpk_detect_endian() {
    short int num = 0x01;
    mpk_endian = !!(*((char *) &num));
}

static UINT32 mpk_swap(UINT32 x) {
    if (mpk_endian == MPAK_BIG_ENDIAN)
        return mpk_swap_uint32(x);
    return x;
}

static bool mpk_crc_built = false;
static UINT32 mpk_crc_table[256];

static void mpk_build_crc_table() {
    UINT32 crc;

    for (int i = 0; i < 256; i++) {
        crc = i;
        for (int j = 8; j > 0; j--) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320L;
            else
                crc >>= 1;
        }
        mpk_crc_table[i] = crc;
    }
}

static UINT32 mpk_compute_crc(UINT32 crc, size_t len, void *buffer) {
    auto *p = static_cast<unsigned char *>(buffer);
    while (len-- != 0) {
        UINT32 tmp1 = (crc >> 8) & 0x00FFFFFFL;
        UINT32 tmp2 = mpk_crc_table[((int) crc ^ *p++) & 0xFF];
        crc = tmp1 ^ tmp2;
    }
    return crc;
}

static UINT32 mpk_crc_file(const char *file, UINT32 pos) {
    UINT32 crc = 0xFFFFFFFFL;

    FILE *fin = fopen(file, "rb");
    if (fin == nullptr)
        return 0;

    fseek(fin, pos, SEEK_SET);

    const size_t buf_size = 16384;
    std::string buffer(buf_size, '\0');

    for (;;) {
        buffer.clear();
        auto len = fread(buffer.data(), sizeof(char), buf_size, fin);
        if (len == 0)
            break;
        crc = mpk_compute_crc(crc, len, buffer.data());
    }
    crc ^= 0xFFFFFFFFL;

    fclose(fin);
    return crc;
}

void MPAK_FILE::init() {
    num_files = 0;
    crc_checksum = 0;
    mode = MPAK_CLOSED;
    fpointer = nullptr;
    filetable_offset = 0;
    strcpy(mpk_file, "null");
    strcpy(override_dir, "null");

    for (int f = 0; f < MPAK_MAX_FILES; f++) {
        files[f][0] = '\0';
        offsets[f] = 0;
        sizes[f] = 0;
    }

    current_file_size = 0;

    mpk_detect_endian();
    if (!mpk_crc_built) {
        mpk_build_crc_table();
        mpk_crc_built = true;
    }
}

int MPAK_FILE::open_mpk(int open_mode, const char *file, const char *override) {

    if (mode != MPAK_CLOSED)
        return 0;
    mode = open_mode;
    if (open_mode != MPAK_READ && open_mode != MPAK_WRITE) {
        mode = MPAK_CLOSED;
        return 0;
    }

    fpointer = fopen(file, open_mode == MPAK_READ ? "rb" : "wb");
    if (fpointer == nullptr) {
        mode = MPAK_CLOSED;
        return 0;
    }

    strcpy(mpk_file, file);
    strcpy(override_dir, "null");

    if (mode == MPAK_WRITE) {
        fputs("MPK1", fpointer);

        // Reserve space for the CRC32 checksum
        UINT32 dummy_crc = 0;
        dummy_crc = mpk_swap(dummy_crc);
        fwrite(&dummy_crc, sizeof(dummy_crc), 1, fpointer);

        // Reserve space for the file table offset
        UINT32 dummy_offset = 0;
        dummy_offset = mpk_swap(dummy_offset);
        fwrite(&dummy_offset, sizeof(dummy_offset), 1, fpointer);

        // OK, we're ready for adding files
        return 1;
    } else {
        // Read the ID
        char id[5];
        memset(id, 0, sizeof(id));
        fread(id, sizeof(char), 4, fpointer);
        if (strcmp(id, "MPK1") != 0) {
            mode = MPAK_CLOSED;
            return 0;
        }

        // Read the CRC32 checksum
        fread(&crc_checksum, sizeof(crc_checksum), 1, fpointer);
        crc_checksum = mpk_swap(crc_checksum);

        // Calculate the CRC32 of the file
        UINT32 crc = mpk_crc_file(mpk_file, 8);
        if (crc != crc_checksum) {
            mode = MPAK_CLOSED;
            return 0;
        }

        // Read the file table offset
        fread(&filetable_offset, sizeof(filetable_offset), 1, fpointer);
        filetable_offset = mpk_swap(filetable_offset);

        // Seek the file table and read it
        fseek(fpointer, filetable_offset, SEEK_SET);

        // Read the number of files
        fread(&num_files, sizeof(num_files), 1, fpointer);
        num_files = mpk_swap(num_files);

        files_map.reserve(num_files * 2);
        // Read the file information
        UINT32 f;
        for (f = 0; f < num_files; f++) {
            auto len = fgetc(fpointer);
            // File name
            fread(files[f], 1, len, fpointer);
            // Offset
            fread(&offsets[f], sizeof(offsets[f]), 1, fpointer);
            offsets[f] = mpk_swap(offsets[f]);

            files_map[std::string(files[f])] = f;
        }
        // Compute the size from the offsets
        for (f = 0; f < num_files - 1; f++)
            sizes[f] = offsets[f + 1] - offsets[f];
        sizes[num_files - 1] = filetable_offset - offsets[num_files - 1];

        fclose(fpointer);
        if (override != nullptr)
            strcpy(override_dir, override);

        return 1;
    }
}

void MPAK_FILE::close_mpk() {
    if (mode != MPAK_READ && mode != MPAK_WRITE)
        return;

    if (mode == MPAK_READ) {
        mode = MPAK_CLOSED;
        return;
    }
    // We're in write mode, so write the file table to the end of the file.
    // First we get the correct file table offset
    // TODO: implement it
}

// Get a pointer to a certain file in the package. It first looks
// from the override directory, and if the file isn't there it looks
// from the package. The user MUST fclose() the pointer himself!
// Returns NULL on failure
FILE *MPAK_FILE::open_file(const char *file) {
    if (mode != MPAK_READ)
        return nullptr;

    if (strcmp(override_dir, "null") != 0) {
        char test_file[256];
        sprintf(test_file, "%s%s", override_dir, file);

        FILE *fin = fopen(test_file, "rb");
        if (fin) {
            fseek(fin, 0, SEEK_SET);
            current_file_size = ftell(fin);
            fseek(fin, 0, SEEK_SET);
            return fin;
        }
    }
    int idx = find_file(file);
    if (idx != -1) {
        FILE *fin = fopen(mpk_file, "rb");
        if (fin == nullptr)
            return nullptr;
        fseek(fin, offsets[idx], SEEK_SET);
        current_file_size = sizes[idx];
        return fin;
    }
    return nullptr;
}

int MPAK_FILE::find_file(const char *file) {
    auto it = files_map.find(file);
    if (it == std::end(files_map))
        return -1;
    return it->second;
}

int MPAK_FILE::extract_file(const char *file, const char *path) {
    FILE *fin = open_file(file);
    if (!fin)
        return 0;

    char file_out[256];
    if (path != nullptr)
        sprintf(file_out, "%s/%s", path, file);
    else
        strcpy(file_out, file);

    FILE *fout = fopen(file_out, "wb");
    if (!fout) {
        fclose(fin);
        return 0;
    }

    // Read in 16kb chunks and split them out
    const UINT32 buf_size = 16384;
    char buffer[buf_size];
    UINT32 bytes_written = 0;
    UINT32 total_bytes = current_file_size;

    for (;;) {
        auto bytes_left = total_bytes - bytes_written;
        if (bytes_left > buf_size)
            bytes_left = buf_size;

        fread(buffer, sizeof(char), bytes_left, fin);
        fwrite(buffer, sizeof(char), bytes_left, fout);
        bytes_written += bytes_left;

        if (bytes_written >= total_bytes)
            break;
    }

    fflush(fout);
    fclose(fin);
    fclose(fout);

    return 0;
}
