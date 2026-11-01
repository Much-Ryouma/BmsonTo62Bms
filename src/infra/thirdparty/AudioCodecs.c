#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
