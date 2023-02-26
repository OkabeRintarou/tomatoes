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

        // Read the file information
        UINT32 f;
        for (f = 0; f < num_files; f++) {
            auto len = fgetc(fpointer);
            // File name
            fread(files[f], 1, len, fpointer);
            // Offset
            fread(&offsets[f], sizeof(offsets[f]), 1, fpointer);
            offsets[f] = mpk_swap(offsets[f]);
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
