CC = /mingw64/bin/gcc
CXX = /mingw64/bin/g++
WINDRES = /mingw64/bin/windres
MOC = /mingw64/bin/moc
ASFLAGS = 
LDFLAGS = 
INCLUDE_DIRS = -I./deps/7zip
LIBRARY_DIRS = 
PACKAGE_NAME = retroarch
BUILD = 
PREFIX = /usr/local
HAVE_7ZIP = 1
HAVE_ACCESSIBILITY = 1
HAVE_AL = 0
HAVE_ALSA = 0
HAVE_ANGLE = 0
HAVE_AUDIOIO = 0
HAVE_AUDIOMIXER = 1
HAVE_AVCODEC = 1
AVCODEC_LIBS = -lavcodec
HAVE_AVDEVICE = 1
AVDEVICE_LIBS = -lavdevice
HAVE_AVFORMAT = 1
AVFORMAT_LIBS = -lavformat
HAVE_AVUTIL = 1
AVUTIL_LIBS = -lavutil
HAVE_AV_CHANNEL_LAYOUT = 1
HAVE_BLISSBOX = 1
HAVE_BLUETOOTH = 0
HAVE_BSV_MOVIE = 1
HAVE_BUILTINBEARSSL = 0
HAVE_BUILTINFLAC = 1
ifneq ($(C89_BUILD),1)
HAVE_BUILTINGLSLANG = 1
endif
ifneq ($(C89_BUILD),1)
HAVE_BUILTINMBEDTLS = 1
endif
HAVE_BUILTINZLIB = 1
HAVE_C99 = 1
C99_CFLAGS = -std=gnu99
HAVE_CACA = 0
HAVE_CC = 1
HAVE_CC_RESAMPLER = 1
HAVE_CDROM = 1
HAVE_CG = 0
ifneq ($(CXX_BUILD),1)
HAVE_CHD = 1
endif
HAVE_CHEATS = 1
HAVE_CHECK = 0
HAVE_CHEEVOS = 1
HAVE_COMMAND = 1
HAVE_CONFIGFILE = 1
HAVE_COREAUDIO3 = 0
HAVE_CORE_INFO_CACHE = 1
ifneq ($(C89_BUILD),1)
HAVE_CRTSWITCHRES = 1
endif
HAVE_CXX = 1
HAVE_CXX11 = 1
CXX11_CFLAGS = -std=c++11
ifneq ($(C89_BUILD),1)
HAVE_D3D10 = 1
endif
ifneq ($(C89_BUILD),1)
HAVE_D3D11 = 1
endif
ifneq ($(C89_BUILD),1)
HAVE_D3D12 = 1
endif
HAVE_D3D8 = 0
ifneq ($(C89_BUILD),1)
HAVE_D3D9 = 1
endif
D3D9_LIBS = -ld3d9
HAVE_D3DX = 1
HAVE_D3DX8 = 0
HAVE_D3DX9 = 1
D3DX9_LIBS = -ld3dx9
HAVE_DBUS = 0
HAVE_DEBUG = 0
HAVE_DINPUT = 1
DINPUT_LIBS = -ldinput8
ifneq ($(C89_BUILD),1)
HAVE_DISCORD = 1
endif
HAVE_DISPMANX = 0
HAVE_DRM = 0
HAVE_DRMINGW = 0
HAVE_DR_MP3 = 1
HAVE_DSOUND = 1
DSOUND_LIBS = -ldsound
HAVE_DSP_FILTER = 1
HAVE_DYLIB = 1
HAVE_DYNAMIC = 1
HAVE_DYNAMIC_EGL = 0
HAVE_EGL = 0
HAVE_EXYNOS = 0
ifneq ($(C89_BUILD),1)
HAVE_FFMPEG = 1
endif
HAVE_FLAC = 0
HAVE_FLOATHARD = 0
HAVE_FLOATSOFTFP = 0
HAVE_FONTCONFIG = 1
FONTCONFIG_CFLAGS = -IC:/msys/mingw64/include/freetype2 -IC:/msys/mingw64/include/libpng16 -IC:/msys/mingw64/include/harfbuzz -IC:/msys/mingw64/include/glib-2.0 -IC:/msys/mingw64/lib/glib-2.0/include -mms-bitfields
FONTCONFIG_LIBS = -lfontconfig -lfreetype
HAVE_FREETYPE = 1
FREETYPE_CFLAGS = -IC:/msys/mingw64/include/freetype2 -IC:/msys/mingw64/include/libpng16 -IC:/msys/mingw64/include/harfbuzz -IC:/msys/mingw64/include/glib-2.0 -IC:/msys/mingw64/lib/glib-2.0/include -mms-bitfields
FREETYPE_LIBS = -lfreetype
HAVE_GBM = 0
HAVE_GDI = 1
HAVE_GETADDRINFO = 1
HAVE_GETOPT_LONG = 0
HAVE_GFX_WIDGETS = 1
HAVE_GLSL = 1
ifneq ($(C89_BUILD),1)
HAVE_GLSLANG = 1
endif
HAVE_GLSLANG_GENERICCODEGEN = 0
HAVE_GLSLANG_HLSL = 0
HAVE_GLSLANG_MACHINEINDEPENDENT = 0
HAVE_GLSLANG_OGLCOMPILER = 0
HAVE_GLSLANG_OSDEPENDENT = 0
HAVE_GLSLANG_SPIRV = 0
HAVE_GLSLANG_SPIRV_TOOLS = 0
HAVE_GLSLANG_SPIRV_TOOLS_OPT = 0
HAVE_HID = 0
HAVE_HLSL = 0
HAVE_IBXM = 1
HAVE_IFINFO = 1
HAVE_IMAGEVIEWER = 1
HAVE_JACK = 0
HAVE_KMS = 0
HAVE_LANGEXTRA = 1
HAVE_LIBCHECK = 0
HAVE_LIBRETRODB = 1
HAVE_LIBSHAKE = 0
HAVE_LIBUSB = 0
HAVE_LUA = 0
HAVE_MALI_FBDEV = 0
HAVE_MEMFD_CREATE = 0
HAVE_MENU = 1
HAVE_METAL = 0
HAVE_MICROPHONE = 1
HAVE_MIST = 0
HAVE_MISTER = 1
HAVE_MMAP = 0
HAVE_MOC = 1
HAVE_MPV = 0
HAVE_NEAREST_RESAMPLER = 1
HAVE_NEON = 0
HAVE_NETPLAYDISCOVERY = 1
HAVE_NETPLAYDISCOVERY = 1
ifneq ($(C89_BUILD),1)
HAVE_NETWORKGAMEPAD = 1
endif
HAVE_NETWORKING = 1
NETWORKING_LIBS = -lws2_32
HAVE_NETWORK_CMD = 1
HAVE_NETWORK_VIDEO = 0
HAVE_NOUNUSED = 1
NOUNUSED_CFLAGS = -Wno-unused-result
HAVE_NOUNUSED_VARIABLE = 1
NOUNUSED_VARIABLE_CFLAGS = -Wno-unused-variable
HAVE_NVDA = 1
HAVE_ODROIDGO2 = 0
HAVE_OMAP = 0
HAVE_ONLINE_UPDATER = 1
HAVE_OPENDINGUX_FBDEV = 0
HAVE_OPENGL = 1
OPENGL_LIBS = -lopengl32
HAVE_OPENGL1 = 1
HAVE_OPENGLES = 0
HAVE_OPENGLES3 = 0
HAVE_OPENGLES3_1 = 0
HAVE_OPENGLES3_2 = 0
ifneq ($(C89_BUILD),1)
HAVE_OPENGL_CORE = 1
endif
HAVE_OPENSSL = 1
OPENSSL_LIBS = -lssl -lcrypto
HAVE_OSMESA = 0
HAVE_OSS = 0
HAVE_OSS_BSD = 0
HAVE_OSS_LIB = 0
HAVE_OVERLAY = 1
HAVE_PARPORT = 0
HAVE_PATCH = 1
HAVE_PLAIN_DRM = 0
HAVE_PRESERVE_DYLIB = 0
HAVE_PULSE = 0
ifneq ($(C89_BUILD),1)
HAVE_QT = 1
endif
HAVE_QT5CONCURRENT = 1
QT5CONCURRENT_CFLAGS = -DQT_CONCURRENT_LIB -IC:/msys/mingw64/include/QtConcurrent -DQT_CORE_LIB -IC:/msys/mingw64/include/QtCore
QT5CONCURRENT_LIBS = -lQt5Concurrent -lQt5Core
HAVE_QT5CORE = 1
QT5CORE_CFLAGS = -DQT_CORE_LIB -IC:/msys/mingw64/include/QtCore
QT5CORE_LIBS = -lQt5Core
HAVE_QT5GUI = 1
QT5GUI_CFLAGS = -DQT_GUI_LIB -IC:/msys/mingw64/include/QtGui -DQT_CORE_LIB -IC:/msys/mingw64/include/QtCore
QT5GUI_LIBS = -lQt5Gui -lQt5Core
HAVE_QT5NETWORK = 1
QT5NETWORK_CFLAGS = -DQT_NETWORK_LIB -IC:/msys/mingw64/include/QtNetwork -DQT_CORE_LIB -IC:/msys/mingw64/include/QtCore
QT5NETWORK_LIBS = -lQt5Network -lQt5Core
HAVE_QT5WIDGETS = 1
QT5WIDGETS_CFLAGS = -DQT_WIDGETS_LIB -IC:/msys/mingw64/include/QtWidgets -IC:/msys/mingw64/include/QtCore -DQT_GUI_LIB -IC:/msys/mingw64/include/QtGui -DQT_CORE_LIB
QT5WIDGETS_LIBS = -lQt5Widgets -lQt5Gui -lQt5Core
HAVE_RBMP = 1
HAVE_REWIND = 1
HAVE_RJPEG = 1
HAVE_ROAR = 0
HAVE_RPNG = 1
HAVE_RSOUND = 0
HAVE_RTGA = 1
HAVE_RUNAHEAD = 1
HAVE_RWAV = 1
HAVE_SAPI = 0
HAVE_SCREENSHOTS = 1
HAVE_SDL = 0
SDL_CFLAGS = -IC:/msys/mingw64/include/SDL -D_GNU_SOURCE=1 -Dmain=SDL_main
SDL_LIBS = -lmingw32 -lSDLmain -lSDL
ifneq ($(C89_BUILD),1)
HAVE_SDL2 = 1
endif
SDL2_CFLAGS = -IC:/msys/mingw64/include/SDL2 -Dmain=SDL_main
SDL2_LIBS = -lmingw32 -lSDL2main -lSDL2 -mwindows
HAVE_SDL_DINGUX = 0
ifneq ($(C89_BUILD),1)
HAVE_SHADERPIPELINE = 1
endif
HAVE_SIXEL = 0
ifneq ($(C89_BUILD),1)
HAVE_SLANG = 1
endif
HAVE_SOCKET_LEGACY = 0
ifneq ($(C89_BUILD),1)
HAVE_SPIRV_CROSS = 1
endif
HAVE_SR2 = 1
HAVE_SSA = 1
SSA_LIBS = -lfribidi -lass
HAVE_SSE = 0
ifneq ($(C89_BUILD),1)
HAVE_SSL = 1
endif
HAVE_STB_FONT = 1
HAVE_STB_IMAGE = 1
HAVE_STB_VORBIS = 1
HAVE_STDIN_CMD = 0
HAVE_STEAM = 0
HAVE_STRCASESTR = 0
HAVE_SUNXI = 0
HAVE_SWRESAMPLE = 1
SWRESAMPLE_LIBS = -lswresample
HAVE_SWSCALE = 1
SWSCALE_LIBS = -lswscale
HAVE_SYSTEMD = 0
HAVE_SYSTEMMBEDTLS = 0
HAVE_THREADS = 1
THREADS_LIBS = -lpthread
HAVE_THREAD_STORAGE = 1
THREAD_STORAGE_LIBS = -lpthread
HAVE_TRANSLATE = 1
HAVE_UDEV = 0
HAVE_UPDATE_ASSETS = 1
HAVE_UPDATE_CORES = 1
HAVE_UPDATE_CORE_INFO = 1
HAVE_V4L2 = 0
HAVE_VC_TEST = 0
HAVE_VG = 0
HAVE_VIDEOCORE = 0
HAVE_VIDEOPROCESSOR = 0
HAVE_VIDEO_FILTER = 1
HAVE_VIVANTE_FBDEV = 0
ifneq ($(C89_BUILD),1)
HAVE_VULKAN = 1
endif
HAVE_VULKAN_DISPLAY = 1
HAVE_WASAPI = 1
HAVE_WAYLAND = 0
HAVE_WAYLAND_CURSOR = 0
HAVE_WAYLAND_PROTOS = 0
HAVE_WAYLAND_SCANNER = 0
HAVE_WIFI = 0
HAVE_WINMM = 1
HAVE_WINRAWINPUT = 1
HAVE_X11 = 0
HAVE_XAUDIO = 1
HAVE_XDELTA = 1
HAVE_XINERAMA = 0
HAVE_XINPUT = 1
HAVE_XKBCOMMON = 0
HAVE_XRANDR = 0
HAVE_XSHM = 0
HAVE_XVIDEO = 0
HAVE_ZLIB = 1
ZLIB_LIBS = -lz
DATA_DIR = /usr/local/share
DYLIB_LIB = 
ASSETS_DIR = /usr/local/share/retroarch
FILTERS_DIR = /usr/local/share/retroarch
CORE_INFO_DIR = /usr/local/share/retroarch
BIN_DIR = /usr/local/bin
DOC_DIR = /usr/local/share/doc/retroarch
MAN_DIR = /usr/local/share/man
OS = Win32
QT_VERSION = qt5
GLOBAL_CONFIG_DIR = /etc
