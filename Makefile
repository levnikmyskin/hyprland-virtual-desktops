# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.

PLUGIN_NAME=virtual-desktops

SOURCE_FILES=$(wildcard src/*.cpp)

COMPILE_FLAGS=-shared -g -fPIC --no-gnu-unique -std=c++23 -Wall
COMPILE_FLAGS+=-Iinclude
INCLUDES = `pkg-config --cflags pixman-1 libdrm hyprland` 

all: $(PLUGIN_NAME).so

$(PLUGIN_NAME).so: $(SOURCE_FILES) $(INCLUDE_FILES)
	g++ -O2 $(COMPILE_FLAGS) $(INCLUDES) $(COMPILE_DEFINES) $(SOURCE_FILES) -o $(PLUGIN_NAME).so

debug: $(SOURCE_FILES) $(INCLUDE_FILES)
	g++ -DDEBUG $(COMPILE_FLAGS) $(INCLUDES) $(COMPILE_DEFINES) $(SOURCE_FILES) -o $(PLUGIN_NAME).so

clean:
	rm -f ./$(PLUGIN_NAME).so

