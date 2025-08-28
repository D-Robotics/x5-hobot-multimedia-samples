include ./makefile.param

.PHONY : all clean
all : install
	@echo "Building WebServer ..."

install:
	-mkdir -p $(GLOBAL_INSTALL_DIR)/bin/www
	cp -af  www/* $(GLOBAL_INSTALL_DIR)/bin/www
	rm -r $(GLOBAL_INSTALL_DIR)/bin/www/js/wasm_video_player/dist/



