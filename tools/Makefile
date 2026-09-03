CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CFLAGS ?= -O1 -w
SRC = ../BreatheBlock/BreathField.cpp ../BreatheBlock/BreathScene.cpp

# --- design review with no LVGL and no hardware -----------------------------
preview: preview.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lm

# --- the real device UI, running on the host --------------------------------
# Point LVGL_DIR at a v9.2.2 checkout:
#   git clone --depth 1 -b v9.2.2 https://github.com/lvgl/lvgl
LVGL_DIR ?= ../lvgl
HOST_UI_SRC = host_ui.cpp ../BreatheBlock/BreathingUI.cpp \
              ../BreatheBlock/StressEngine.cpp \
              ../BreatheBlock/DemoDirector.cpp $(SRC)

host/liblvgl.a: host/lv_conf.h
	@test -d $(LVGL_DIR)/src || \
	  { echo "LVGL_DIR=$(LVGL_DIR) has no src/; see the comment above"; exit 1; }
	@rm -rf host/obj && mkdir -p host/obj
	@find $(LVGL_DIR)/src -name '*.c' \
	  | grep -vE 'drivers/(sdl|glfw|opengles|x11|wayland|libinput|evdev|fbdev|drm|nuttx|windows|uefi|st|renesas|arm2d|display/lcd)|libs/(rlottie|ffmpeg|freetype|libpng|libjpeg|tjpgd|gif|barcode|qrcode|tiny_ttf)|others/vg_lite' \
	  > host/srcs.txt
	@echo "compiling $$(wc -l < host/srcs.txt) LVGL sources..."
	@while read f; do \
	  $(CC) -c $(CFLAGS) -I$(LVGL_DIR) -Ihost -DLV_CONF_INCLUDE_SIMPLE \
	    "$$f" -o host/obj/$$(echo "$$f" | md5sum | cut -c1-12).o || exit 1; \
	 done < host/srcs.txt
	@ar rcs $@ host/obj/*.o

host_ui: $(HOST_UI_SRC) host/liblvgl.a
	$(CXX) $(CXXFLAGS) -DLV_CONF_INCLUDE_SIMPLE -I$(LVGL_DIR) -Ihost \
	  -o $@ $(HOST_UI_SRC) host/liblvgl.a -lm

clean:
	rm -f preview host_ui preview_port_dump host/liblvgl.a host/srcs.txt
	rm -rf host/obj

.PHONY: clean

# --- keep the browser preview honest ----------------------------------------
preview_port_dump: preview_port_dump.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lm

check-preview: preview_port_dump
	node check_preview_port.mjs
