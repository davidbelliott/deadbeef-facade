#-Wall -O3
CFLAGS 	=  -g -lm -D_GNU_SOURCE -I./inc -I./src -Iwhitgl/inc -Iwhitgl/input/TinyMT -O3
LDFLAGS =  -Wl,-rpath=.,--enable-new-dtags -Lwhitgl/build/lib -Lwhitgl/input/irrklang/bin/linux-gcc-64  -Lwhitgl/input/glfw/build/src -l:whitgl.a -l:libglfw3.a -lGLU -lGL -lGLEW -lm -lIrrKlang -lX11 -lXxf86vm -lpthread -lXrandr -lXinerama -lXcursor -lXi -lpng -ldl whitgl/input/TinyMT/tinymt/tinymt64.o -lz -lstdc++

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
MODEL_OBJ = $(wildcard data/obj/*.obj)
MODEL_WMD = $(MODEL_OBJ:.obj=.wmd)

PROGRAM = dbbp
PNG = data/tex/tex.png $(wildcard data/lvl/lvl*.png)

all: $(PROGRAM) $(MODEL_WMD) $(PNG)

$(PROGRAM): $(OBJ)
	gcc -o $@ $^ $(LDFLAGS)

%.o: src/%.c
	gcc -o $@ $^ $(CFLAGS)

$(MODEL_WMD): $(MODEL_OBJ)
	./whitgl/scripts/process_model.py $(@:.wmd=.obj) $@ 

%.png: %.bmp
	convert $< $@

.PHONY: clean
clean:
	rm -f $(OBJ) $(PROGRAM) $(MODEL_WMD)
