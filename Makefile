
SDL_CFLAGS   := `sdl2-config --cflags`
SDL_LIBS     := `sdl2-config --libs`

DL_LIBS      := -ldl
MODPLUG_LIBS := -lmodplug
TREMOR_LIBS  := -lvorbisidec -logg
ZLIB_LIBS    := -lz

CXXFLAGS += -Wall -Wpedantic -MMD $(SDL_CFLAGS) -DUSE_MODPLUG -DUSE_STATIC_SCALER -DUSE_TREMOR -DUSE_ZLIB

SRCS = collision.cpp cpc_player.cpp cutscene.cpp decode_mac.cpp dynlib.cpp file.cpp fs.cpp game.cpp graphics.cpp main.cpp \
	menu.cpp mixer.cpp mod_player.cpp ogg_player.cpp piege.cpp protection.cpp resource.cpp resource_aba.cpp \
	resource_mac.cpp scaler.cpp screenshot.cpp seq_player.cpp \
	sfx_player.cpp staticres.cpp systemstub_sdl.cpp unpack.cpp util.cpp video.cpp

SCALERS := scalers/scaler_nearest.cpp scalers/scaler_tv2x.cpp scalers/scaler_xbr.cpp

OBJS = $(SRCS:.cpp=.o) $(SCALERS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d) $(SCALERS:.cpp=.d)

LIBS = $(SDL_LIBS) $(DL_LIBS) $(MODPLUG_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS)

rs: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)
