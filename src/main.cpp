#include "./common.h"
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <uri> 1/0\n", argv[0]);
        return 1;
    }

    std::thread sdr_thread([argc, argv]() {
        sdr_run(argc, argv);
    });
    
    if (strcmp(argv[2], "0") == 0) {
        std::thread gui_thread(run_gui);
        gui_thread.join();
    }
    
    sdr_thread.join();
    
    return 0;
}