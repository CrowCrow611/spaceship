#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>

Config cfg;

void Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Config: could not open " << path << ", using defaults\n";
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        //skip comments, empty lines
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        //trim whitespace
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end   = s.find_last_not_of(" \t\r\n");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };
        trim(key); trim(value);
        data[key] = value;
    }

    std::cout << "Config: loaded " << data.size() << " settings from " << path << "\n";
}

int Config::getInt(const std::string& key, int def) const {
    auto it = data.find(key);
    return it != data.end() ? std::stoi(it->second) : def;
}

float Config::getFloat(const std::string& key, float def) const {
    auto it = data.find(key);
    return it != data.end() ? std::stof(it->second) : def;
}

bool Config::getBool(const std::string& key, bool def) const {
    auto it = data.find(key);
    if (it == data.end()) return def;
    return it->second == "true" || it->second == "1";
}

std::string Config::getString(const std::string& key, const std::string& def) const {
    auto it = data.find(key);
    return it != data.end() ? it->second : def;
}

