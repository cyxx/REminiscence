
SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs`
MODPLUG_LIBS = -lmodplug
# TREMOR_LIBS = -lvorbisidec -logg
ZLIB_LIBS = -lz

CXX := clang++
CXXFLAGS := -Wall -MMD $(SDL_CFLAGS) -DUSE_MODPLUG -DUSE_ZLIB # -DUSE_TREMOR

SRCS = collision.cpp cutscene.cpp file.cpp fs.cpp game.cpp graphics.cpp main.cpp menu.cpp \
	mixer.cpp mod_player.cpp ogg_player.cpp piege.cpp resource.cpp resource_aba.cpp \
	scaler.cpp seq_player.cpp \
	sfx_player.cpp staticres.cpp systemstub_sdl.cpp unpack.cpp util.cpp video.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

LIBS = $(SDL_LIBS) $(MODPLUG_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS)

rs: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f *.o *.d

-include $(DEPS)
