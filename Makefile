# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.

.PHONY: all debug clean

BUILD_DIR = build

all: 
	mkdir $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release ..
	cmake --build $(BUILD_DIR)
	cp $(BUILD_DIR)/libhyprland-virtual-desktops.so ./virtual-desktops.so

debug: 
	mkdir $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug ..
	cmake --build $(BUILD_DIR)
	cp $(BUILD_DIR)/libhyprland-virtual-desktops.so ./virtual-desktops.so

clean:
	rm -rf $(BUILD_DIR) virtual-desktops.so
