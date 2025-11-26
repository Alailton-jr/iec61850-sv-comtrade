#include <iostream>
#include "app.h"
#include <iostream>
#include <csignal>

int main(int argc, char* argv[]) {
    std::cout << "IEC 61850 SV COMTRADE Application" << std::endl;
    App app;
    return app.run(argc, argv);
}
