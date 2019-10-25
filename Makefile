CFLAGS  = $(shell pkg-config sdl2 --cflags)
#-Wall -O3
CFLAGS +=  -Werror -std=c11 -lm -D_GNU_SOURCE -g -Iwhitgl/inc -Iwhitgl/input/TinyMT

LDFLAGS = -lGL -lGLEW -lX11 -lpthread -lXrandr -lXi -ldl -L. -Lwhitgl/build/lib -l:whitgl.a -Lwhitgl/input/irrklang -Lwhitgl/input/irrklang/bin/linux-gcc-64 -lIrrKlang -lpng -l:whitgl/input/TinyMT/tinymt/tinymt64.o
LDFLAGS += "-Wl,-rpath,."
LDFLAGS += $(shell pkg-config --libs glfw3)
LDFLAGS += $(shell pkg-config sdl2 --libs)
LDFLAGS += $(shell pkg-config SDL2_image --libs)

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

INC = -I./inc -I./src
PROGRAM = dbbp

$(PROGRAM): $(SRC)
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS) $(INC)

.PHONY: clean
clean:
	rm -f $(OBJ) $(PROGRAM)
