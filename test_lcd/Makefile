obj-m += test_fb.o
test_fb-objs := test.o fb_model.o
KSRC := ../../../ebf-buster-linux/build_image/build
export ARCH := arm
export CROSS_COMPILE := arm-linux-gnueabihf-
all:
	make -C $(KSRC) modules M=`pwd` 


.PHONY : clean
clean:
	make -C $(KSRC) modules clean M=`pwd` 

