
SDL_CFLAGS   := `sdl2-config --cflags`
SDL_LIBS     := `sdl2-config --libs`

MODPLUG_LIBS := -lmodplug
TREMOR_LIBS  := #-lvorbisidec -logg
ZLIB_LIBS    := -lz

LIBS = $(SDL_LIBS) $(MODPLUG_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS)

CXXFLAGS += -Wall -Wextra -Wno-unused-parameter -Wpedantic -MMD $(SDL_CFLAGS) -DUSE_MODPLUG -DUSE_STB_VORBIS -DUSE_ZLIB

SRCS = collision.cpp cpc_player.cpp cutscene.cpp decode_mac.cpp file.cpp fs.cpp game.cpp graphics.cpp main.cpp \
	menu.cpp midi_parser.cpp mixer.cpp mod_player.cpp ogg_player.cpp \
	piege.cpp prf_player.cpp protection.cpp resource.cpp resource_aba.cpp \
	resource_mac.cpp resource_paq.cpp scaler.cpp screenshot.cpp seq_player.cpp \
	sfx_player.cpp staticres.cpp systemstub_sdl.cpp unpack.cpp util.cpp video.cpp

#CXXFLAGS += -DUSE_STATIC_SCALER
#SCALERS  := scalers/scaler_nearest.cpp scalers/scaler_tv2x.cpp scalers/scaler_xbr.cpp

#CXXFLAGS    += -DUSE_MIDI_DRIVER
#MIDIDRIVERS := midi_driver_adlib.cpp midi_driver_mt32.cpp
#MIDI_LIBS   := -lmt32emu

LIBS = $(MIDI_LIBS) $(MODPLUG_LIBS) $(SDL_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS)

OBJS = $(SRCS:.cpp=.o) $(SCALERS:.cpp=.o) $(MIDIDRIVERS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d) $(SCALERS:.cpp=.d) $(MIDIDRIVERS:.cpp=.d)

rs: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)
