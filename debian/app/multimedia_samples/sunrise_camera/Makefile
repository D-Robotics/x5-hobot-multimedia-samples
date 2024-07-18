include ./makefile.param

TARGET_DIR = sunrise_camera

COMPILE_TIME = $(shell date +"%Y%M%d%H%M%S")
TARBALL="${ROOT}${TARGET_DIR}/${TARGET_DIR}_${COMPILE_PREFIX}${COMPILE_TIME}.tar.gz"

rel:
	@mkdir -p sunrise_camera
	@cd sunrise_camera; mkdir -p lib bin include;cd ../;
	@for dir in $(subdir); do \
		$(MAKE) -C $$dir || exit "$$?"; \
	done

clean:
	@for dir in $(subdir); do \
		$(MAKE) -C $$dir clean || exit "$$?"; \
	done
	@cd sunrise_camera;rm -rf lib bin include model_zoom  test_data
	@rm -rf sunrise_camera
	@rm -rf out/

sdk:rel
	@echo "build ok!!!"

install:
	@echo "Generate deployment package"
	@mkdir -p out
	@tar -czvf out/sunrise_camera_v3.1.0.tar.gz sunrise_camera WebServer start_app.sh

cleansdk:
	@rm -Rf $(shell find ${TARGET_DIR}/include -name *.h)
	@rm -Rf $(shell find ${TARGET_DIR}/lib -name *.a)
