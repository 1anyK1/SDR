#include "./common.h"

int main(int argc, char* argv[]) {

    std::thread sdr_thread([argc, argv]() {
        sdr_run(argc, argv);
    });
    std::thread gui_thread(run_gui);
    sdr_thread.join();
    gui_thread.join();

}