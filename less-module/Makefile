obj-m += less.o
less-objs := main.o less_file.o ydump.o yresume.o

#lessdev-objs := less.o main.o ydump.o

all:
	make -C /Extra/shaneed/Source/build-23-dev M=$(PWD) modules

clean:
	make -C /lib/modules/2.6.23-dev/build M=$(PWD) clean


