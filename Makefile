
#DEFINES := -DUSE_MIXER_IMPL
CXXFLAGS := -g -MMD -Wall `sdl-config --cflags` $(DEFINES)

OBJS = debug.o disasm.o file.o funcs.o gl_cursor.o gl_texture.o game.o main.o memory.o mixer.o real.o screenshot.o script.o segment_exe.o stub.o text.o traps.o
DEPS = $(OBJS:.o=.d)

igor: $(OBJS)
	$(CXX) -o $@ $^ `sdl-config --libs` -lvorbisidec -lz -lGL

clean:
	rm -f *.d *.o

-include $(DEPS)
