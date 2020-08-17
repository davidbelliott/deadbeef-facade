#-Wall -O3
CFLAGS 	=  -Werror -lm -D_GNU_SOURCE -g -pg -I./inc -I./src -Iwhitgl/inc -Iwhitgl/input/TinyMT -Imidi-parser/include
LDFLAGS =  -Wl,-rpath=.,--enable-new-dtags -Lmidi-parser -Lwhitgl/build/lib -Lwhitgl/input/irrklang/bin/linux-gcc-64  -Lwhitgl/input/glfw/build/src -lmidi-parser -l:whitgl.a -lglfw3 -lGLU -lGL -lGLEW -lm -lIrrKlang -lX11 -lXxf86vm -lpthread -lXrandr -lXinerama -lXcursor -lXi -lpng -ldl whitgl/input/TinyMT/tinymt/tinymt64.o -lz -lstdc++

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
MODEL_OBJ = $(wildcard data/obj/*.obj)
MODEL_WMD = $(MODEL_OBJ:.obj=.wmd)

PROGRAM = dbbp

all: $(PROGRAM) $(MODEL_WMD) data/tex/tex.png

$(PROGRAM): $(OBJ)
	gcc -o $@ $^ $(LDFLAGS)

%.o: src/%.c
	gcc -o $@ $^ $(CFLAGS)

$(MODEL_WMD): $(MODEL_OBJ)
	./whitgl/scripts/process_model.py $(@:.wmd=.obj) $@ 

data/tex/tex.png: data/tex/tex.bmp
	convert data/tex/tex.bmp data/tex/tex.png

.PHONY: clean
clean:
	rm -f $(OBJ) $(PROGRAM) $(MODEL_WMD)
