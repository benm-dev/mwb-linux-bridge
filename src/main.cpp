#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>
#include "NetworkManager.h"
#include "InputManager.h"

namespace {

struct AppConfig {
    std::string windowsIp;
    int port{15101};
    std::string securityKeyFile;
};

std::string StripWhitespace(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), value.end());
    return value;
}

std::string Trim(std::string value) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) {
        return !isSpace(c);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) {
        return !isSpace(c);
    }).base(), value.end());
    return value;
}

std::string ExpandUserPath(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (!home) return path;
    if (path.size() == 1) return home;
    if (path[1] == '/') return std::string(home) + path.substr(1);
    return path;
}

std::string DefaultConfigPath() {
    if (const char* xdgConfig = std::getenv("XDG_CONFIG_HOME")) {
        return std::string(xdgConfig) + "/mwb-linux-bridge/config";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::string(home) + "/.config/mwb-linux-bridge/config";
    }
    return "mwb-linux-bridge.conf";
}

bool ParsePortValue(const std::string& value, int& port) {
    errno = 0;
    char* end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0' || parsed < 1 || parsed > 65535) {
        return false;
    }
    port = static_cast<int>(parsed);
    return true;
}

bool ParsePort(const char* value, int& port) {
    return ParsePortValue(value, port);
}

bool LoadConfigFile(const std::string& path, AppConfig& config) {
    std::ifstream file(path);
    if (!file) return false;

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        lineNumber++;
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t equals = line.find('=');
        if (equals == std::string::npos) {
            std::cerr << "WARN: Ignoring invalid config line " << lineNumber << " in " << path << std::endl;
            continue;
        }

        std::string key = Trim(line.substr(0, equals));
        std::string value = Trim(line.substr(equals + 1));
        if (key == "windows_ip" || key == "host") {
            config.windowsIp = value;
        } else if (key == "port") {
            int parsedPort = 0;
            if (!ParsePortValue(value, parsedPort)) {
                std::cerr << "WARN: Ignoring invalid port in " << path << ": " << value << std::endl;
                continue;
            }
            config.port = parsedPort;
        } else if (key == "security_key_file") {
            config.securityKeyFile = ExpandUserPath(value);
        } else {
            std::cerr << "WARN: Unknown config key in " << path << ": " << key << std::endl;
        }
    }

    return true;
}

bool EnsureConfigDirectory(const std::string& configPath) {
    size_t slash = configPath.rfind('/');
    if (slash == std::string::npos) return true;

    std::string current;
    std::string dir = configPath.substr(0, slash);
    if (dir.empty()) return true;
    if (dir[0] == '/') current = "/";

    size_t start = (dir[0] == '/') ? 1 : 0;
    while (start <= dir.size()) {
        size_t next = dir.find('/', start);
        std::string part = dir.substr(start, next == std::string::npos ? std::string::npos : next - start);
        if (!part.empty()) {
            if (!current.empty() && current.back() != '/') current += "/";
            current += part;
            if (mkdir(current.c_str(), 0700) != 0 && errno != EEXIST) {
                return false;
            }
        }
        if (next == std::string::npos) break;
        start = next + 1;
    }
    return true;
}

bool InitConfigFile(const std::string& path) {
    std::string expanded = ExpandUserPath(path);
    if (!EnsureConfigDirectory(expanded)) {
        std::cerr << "ERROR: Could not create config directory for " << expanded << std::endl;
        return false;
    }

    std::ifstream existing(expanded);
    if (existing.good()) {
        std::cerr << "Config already exists: " << expanded << std::endl;
        return true;
    }

    std::ofstream file(expanded, std::ios::out | std::ios::trunc);
    if (!file) {
        std::cerr << "ERROR: Could not write config file: " << expanded << std::endl;
        return false;
    }

    file << "# MWB Linux Bridge config\n"
         << "# Run: mwb-client\n"
         << "windows_ip=192.168.1.10\n"
         << "port=15101\n"
         << "# Optional. Prefer chmod 600 on the key file if you use this.\n"
         << "# security_key_file=~/.config/mwb-linux-bridge/key\n";
    file.close();
    chmod(expanded.c_str(), 0600);
    std::cout << "Wrote config: " << expanded << std::endl;
    return true;
}

std::string ReadFirstLine(const std::string& path) {
    std::string expanded = ExpandUserPath(path);
    struct stat st {};
    if (stat(expanded.c_str(), &st) == 0 && (st.st_mode & 0077) != 0) {
        std::cerr << "WARN: Security key file is readable by group/others; run: chmod 600 " << expanded << std::endl;
    }

    std::ifstream file(expanded);
    if (!file) {
        std::cerr << "WARN: Could not read security key file: " << expanded << std::endl;
        return "";
    }

    std::string key;
    std::getline(file, key);
    return StripWhitespace(key);
}

std::string ReadSecurityKey(const std::string& keyFile) {
    if (const char* envKey = std::getenv("MWB_SECURITY_KEY")) {
        return StripWhitespace(envKey);
    }

    if (!keyFile.empty()) {
        std::string key = ReadFirstLine(keyFile);
        if (!key.empty()) return key;
    }

    std::cerr << "Security key: ";
    termios oldTerm{};
    termios newTerm{};
    bool hidden = false;
    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &oldTerm) == 0) {
        newTerm = oldTerm;
        newTerm.c_lflag &= ~ECHO;
        hidden = tcsetattr(STDIN_FILENO, TCSAFLUSH, &newTerm) == 0;
    }

    std::string key;
    std::getline(std::cin, key);

    if (hidden) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldTerm);
        std::cerr << std::endl;
    }

    return StripWhitespace(key);
}

void PrintUsage() {
    std::cerr << "[Usage]: mwb-client [--config PATH] [--host WINDOWS_IP] [--port 15101]" << std::endl;
    std::cerr << "         mwb-client <WINDOWS_IP> [PORT]" << std::endl;
    std::cerr << "         mwb-client --init-config [--config PATH]" << std::endl;
    std::cerr << "  Default config: " << DefaultConfigPath() << std::endl;
    std::cerr << "  Key source order: MWB_SECURITY_KEY, security_key_file, hidden prompt." << std::endl;
    std::cerr << "  Port 15101 = keyboard/mouse. Port 15100 = clipboard only; do not use it here." << std::endl;
}

bool ParseXrandrCurrentSize(const std::string& output, int& width, int& height) {
    size_t pos = output.find("current ");
    if (pos == std::string::npos) return false;
    const char* current = output.c_str() + pos + 8;

    errno = 0;
    char* end = nullptr;
    long parsedWidth = std::strtol(current, &end, 10);
    if (errno != 0 || end == current || parsedWidth <= 0 || parsedWidth > 65535) return false;

    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) end++;
    if (*end != 'x') return false;
    end++;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) end++;

    errno = 0;
    char* heightEnd = nullptr;
    long parsedHeight = std::strtol(end, &heightEnd, 10);
    if (errno != 0 || heightEnd == end || parsedHeight <= 0 || parsedHeight > 65535) return false;

    width = static_cast<int>(parsedWidth);
    height = static_cast<int>(parsedHeight);
    return true;
}

bool DetectScreenSize(int& width, int& height) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return false;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        int devNull = open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            dup2(devNull, STDERR_FILENO);
            close(devNull);
        }

        if (!std::getenv("DISPLAY")) {
            setenv("DISPLAY", ":0", 0);
        }
        execl("/usr/bin/xrandr", "xrandr", "--current", static_cast<char*>(nullptr));
        _exit(127);
    }

    close(pipefd[1]);
    std::string output;
    char buffer[4096];
    while (true) {
        ssize_t n = read(pipefd[0], buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        output.append(buffer, static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    while (true) {
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid) break;
        if (waited < 0 && errno == EINTR) continue;
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return false;

    return ParseXrandrCurrentSize(output, width, height);
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "--- MWB Linux Bridge (C++17) ---" << std::endl;

    AppConfig config;
    std::string configPath = DefaultConfigPath();
    bool initConfig = false;
    bool hostSetByCli = false;
    bool portSetByCli = false;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        } else if (arg == "--init-config") {
            initConfig = true;
        } else if (arg == "--config") {
            if (++i >= argc) {
                std::cerr << "ERROR: --config needs a path." << std::endl;
                return 1;
            }
            configPath = ExpandUserPath(argv[i]);
        } else if (arg == "--host") {
            if (++i >= argc) {
                std::cerr << "ERROR: --host needs a Windows IP or hostname." << std::endl;
                return 1;
            }
            config.windowsIp = argv[i];
            hostSetByCli = true;
        } else if (arg == "--port") {
            if (++i >= argc || !ParsePort(argv[i], config.port)) {
                std::cerr << "ERROR: --port needs a numeric port from 1 to 65535." << std::endl;
                return 1;
            }
            portSetByCli = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "ERROR: Unknown option: " << arg << std::endl;
            PrintUsage();
            return 1;
        } else {
            positional.push_back(arg);
        }
    }

    if (initConfig) {
        return InitConfigFile(configPath) ? 0 : 1;
    }

    AppConfig fileConfig;
    if (LoadConfigFile(configPath, fileConfig)) {
        if (!hostSetByCli) config.windowsIp = fileConfig.windowsIp;
        if (!portSetByCli) config.port = fileConfig.port;
        if (config.securityKeyFile.empty()) config.securityKeyFile = fileConfig.securityKeyFile;
    }

    if (!positional.empty()) {
        if (hostSetByCli) {
            std::cerr << "ERROR: Use either --host or positional WINDOWS_IP, not both." << std::endl;
            return 1;
        }
        config.windowsIp = positional[0];
    }
    if (positional.size() >= 2 && !ParsePort(positional[1].c_str(), config.port)) {
        std::cerr << "ERROR: Second positional argument must be a numeric port, not the security key." << std::endl;
        std::cerr << "Run without the key argument and enter the key at the hidden prompt." << std::endl;
        return 1;
    }
    if (positional.size() > 2) {
        std::cerr << "ERROR: Too many positional arguments." << std::endl;
        PrintUsage();
        return 1;
    }
    if (config.windowsIp.empty()) {
        std::cerr << "ERROR: Windows IP/host is not configured." << std::endl;
        std::cerr << "Run `mwb-client --init-config`, then edit " << configPath << "." << std::endl;
        return 1;
    }

    std::string key = ReadSecurityKey(config.securityKeyFile);
    if (key.length() < 16) {
        std::cerr << "ERROR: Security key must be at least 16 characters after spaces are removed." << std::endl;
        return 1;
    }

    mwb::InputManager input;
    if (!input.Initialize()) {
        std::cerr << "WARN: Virtual Input pipeline failure. Input injection will not work (run as root), but networking will proceed for testing." << std::endl;
    }

    mwb::NetworkManager network(config.windowsIp, config.port, key);

    int w = 0;
    int h = 0;
    if (DetectScreenSize(w, h)) {
        network.SetScreenSize(w, h);
        input.SetScreenSize(w, h);
        printf("[INFO] Screen size: %dx%d\n", w, h);
    }

    network.SetOnMouseCallback([&](const mwb::MouseData& md) {
        std::cout << "[INPUT] Mouse: x=" << md.x << " y=" << md.y << " dwFlags=0x" << std::hex << md.dwFlags << std::dec << std::endl;
        input.InjectMouse(md);
    });

    network.SetOnKeyboardCallback([&](const mwb::KeyboardData& kd) {
        std::cout << "[INPUT] Keyboard: vk=0x" << std::hex << kd.wVk << " flags=0x" << kd.dwFlags << std::dec << std::endl;
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
