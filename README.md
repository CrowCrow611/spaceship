# spaceship

Small OpenGL-based space sim with a custom virtual CPU (LS32), assembler, and in-game terminal.

## Run (recommended)

Download the AppImage from Releases:

```bash
chmod +x Spaceship-x86_64.AppImage
./Spaceship-x86_64.AppImage
```

## Build (Linux)

Requirements:
```bash
sudo apt install build-essential libgl1-mesa-dev libglfw3-dev
```

Build:
```bash
make
```

Run:
```bash
cd spaceship
./spaceship
```

Note: must be run from the project directory (uses relative asset paths).

# Features

- OpenGL rendering
- LS32 CPU emulator
- assembler + disassembler
- in-game terminal

# Note
To use the ingame cpu, please build it youself, and then mess around the programs, ls32/programs/test.ls

For example:

```
load ls32/programs/test.ls
```
then:
```
run
```
```
r1
```
