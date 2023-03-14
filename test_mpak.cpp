#include <cstdio>
#include "mpak.h"

int main() {
    MPAK_FILE pak;
    pak.init();

    if (!pak.open_mpk(MPAK_READ, "res/tomatoes.mpk", "null")) {
        fprintf(stderr, "Unable to open 'tomatoes.mpk'.\nThe file either doesn't exist or is corrupted.");
        exit(-1);
    }

    for (const auto &it : pak.files_map) {
        pak.extract_file(it.first.data(), "output");
    }

    pak.close_mpk();
    return 0;
}
