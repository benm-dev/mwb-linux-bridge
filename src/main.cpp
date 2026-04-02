#include <iostream>
#include <string>
#include "NetworkManager.h"
#include "InputManager.h"

int main(int argc, char** argv) {
    std::cout << "--- Mouse Without Borders Linux Client (C++17) ---" << std::endl;

    if (argc < 3) {
        std::cerr << "[Usage]: mwb_client <Windows_IP> <Security_Key> [Port=15101]" << std::endl;
        std::cerr << "  Port 15101 = keyboard/mouse (message server)" << std::endl;
        std::cerr << "  Port 15100 = clipboard only -- do NOT use for input control" << std::endl;
        return 1;
    }

    std::string ip = argv[1];
    std::string key = argv[2];
    int port = (argc >= 4) ? std::stoi(argv[3]) : 15101;

    mwb::InputManager input;
    if (!input.Initialize()) {
        std::cerr << "WARN: Virtual Input pipeline failure. Input injection will not work (run as root), but networking will proceed for testing." << std::endl;
    }

    mwb::NetworkManager network(ip, port, key);

    // Let NetworkManager query the real screen size for the identity broadcast
    {
        FILE* f = popen("DISPLAY=:0 xrandr 2>/dev/null | grep 'current' | sed 's/.*current //;s/,.*//'", "r");
        if (f) {
            int w=0, h=0;
            if (fscanf(f, " %d x %d", &w, &h) == 2 && w > 0 && h > 0) {
                network.SetScreenSize(w, h);
                printf("[INFO] Screen size: %dx%d\n", w, h);
            }
            pclose(f);
        }
    }

    network.SetOnMouseCallback([&](const mwb::MouseData& md) {
        std::cout << "[INPUT] Mouse: x=" << md.x << " y=" << md.y << " wParam=0x" << std::hex << md.wParam << std::dec << std::endl;
        input.InjectMouse(md);
    });

    network.SetOnKeyboardCallback([&](const mwb::KeyboardData& kd) {
        std::cout << "[INPUT] Keyboard: vk=0x" << std::hex << kd.vkCode << " flags=0x" << kd.flags << std::dec << std::endl;
        input.InjectKeyboard(kd);
    });

    if (!network.Connect()) {
        std::cerr << "Terminating: Network failure." << std::endl;
        return 1;
    }

    // Enter blocking receive pump
    network.RunLoop();

    return 0;
}
