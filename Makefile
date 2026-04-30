CXX      = g++
CXXFLAGS = -std=c++17 -I include -I ls32 -I lilshit -I /usr/include/freetype2
LDFLAGS  = -lglfw -lGL -ldl -lpthread -lfreetype

SRC = src/main.cpp src/glad.c src/renderer.cpp src/cockpit.cpp src/hud.cpp src/terminal.cpp src/config.cpp src/savegame.cpp src/entitymanager.cpp src/scenenode.cpp src/planets.cpp \
	  ls32/ls32cpu.cpp ls32/ls32dis.cpp \
	  lilshit/ls32asm.cpp
OUT = spaceship

all:
	$(CXX) $(SRC) $(CXXFLAGS) $(LDFLAGS) -o $(OUT)

clean:
	rm -f $(OUT)