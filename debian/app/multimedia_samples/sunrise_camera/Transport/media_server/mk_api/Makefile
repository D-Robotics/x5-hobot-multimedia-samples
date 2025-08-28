include ./makefile.param

.PHONY : all clean

all : $(TARGET) install

install: $(TARGET)
	-mkdir -p $(GLOBAL_INSTALL_DIR)/lib
	cp lib/*.a $(GLOBAL_INSTALL_DIR)/lib

	# -mkdir -p $(GLOBAL_INSTALL_DIR)/include
	# cp include/* $(GLOBAL_INSTALL_DIR)/include

sinclude $(DEP)
