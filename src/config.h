#pragma once
#include <string>
#include <map>

//reads a simple key = value config file
// lines starting wiht # r comments
struct Config {
    std::map<std::string, std::string> data;

    void load(const std::string& path);

    //Getters with defaults
    int getInt (const std::string& key, int def) const;
    float getFloat (const std::string& key, float def) const;
    bool getBool (const std::string& key, bool def) const;
    std::string getString(const std::string& key, const std::string& def) const;
};

//global config instances
extern Config cfg;
