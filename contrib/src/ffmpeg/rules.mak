# FFmpeg

#FFMPEG_SNAPURL := http://git.videolan.org/?p=ffmpeg.git;a=snapshot;h=HEAD;sf=tgz
FFMPEG_SNAPURL := http://git.libav.org/?p=libav.git;a=snapshot;h=HEAD;sf=tgz

FFMPEGCONF = \
	--cc="$(CC)" \
	--disable-doc \
	--disable-decoder=bink \
	--disable-encoder=vorbis \
	--enable-libgsm \
	--enable-libopenjpeg \
	--disable-debug \
	--disable-avdevice \
	--disable-devices \
	--disable-avfilter \
	--disable-filters \
	--disable-bsfs \
	--disable-bzlib

# Those tools are named differently in FFmpeg and Libav
#	--disable-ffserver \
#	--disable-ffplay \
#	--disable-ffprobe
DEPS_ffmpeg = zlib gsm openjpeg

# Optional dependencies
ifdef BUILD_ENCODERS
FFMPEGCONF += --enable-libmp3lame --enable-libvpx --disable-decoder=libvpx
DEPS_ffmpeg += lame $(DEPS_lame) vpx $(DEPS_vpx)
else
FFMPEGCONF += --disable-encoders --disable-muxers
endif

# Small size
ifdef ENABLE_SMALL
FFMPEGCONF += --enable-small
ifeq ($(ARCH),arm)
ifdef HAVE_ARMV7A
# XXX: assumes > ARMv7-A, and thus thumb2-able
FFMPEGCONF += --enable-thumb
endif
endif
endif

ifdef HAVE_CROSS_COMPILE
FFMPEGCONF += --enable-cross-compile
ifndef HAVE_IOS
FFMPEGCONF += --cross-prefix=$(HOST)-
endif
endif

# ARM stuff
ifeq ($(ARCH),arm)
FFMPEGCONF += --arch=arm
ifdef HAVE_NEON
FFMPEGCONF += --cpu=cortex-a8 --enable-neon
FFMPEG_CFLAGS += -mfpu=neon
endif
endif

# x86 stuff
ifeq ($(ARCH),i386)
ifndef HAVE_DARWIN_OS
FFMPEGCONF += --arch=x86
endif
endif

# Darwin
ifdef HAVE_DARWIN_OS
FFMPEGCONF += --arch=$(ARCH) --target-os=darwin
ifneq ($(findstring $(ARCH),i386 x86_64),)
FFMPEGCONF += --enable-memalign-hack
endif
ifeq ($(ARCH),x86_64)
FFMPEGCONF += --cpu=core2
endif
endif
ifdef HAVE_IOS
ifeq ($(ARCH),arm)
FFMPEGCONF += --enable-pic --as="$(AS)"
endif
endif

# Linux
ifdef HAVE_LINUX
FFMPEGCONF += --target-os=linux --enable-pic

# Android x86
ifeq ($(ANDROID_ABI), x86)
ifdef HAVE_ANDROID
# Android-x86 gcc doesn't guarantee an aligned stack, but this is
# handled by __attribute__((force_align_arg_pointer)) in libavcodec
# already, so we tell gcc to assume this alignment, so we don't need
# to waste a precious register in assembly functions to realign it.
FFMPEG_CFLAGS += -mincoming-stack-boundary=4
endif # HAVE_ANDROID
endif # x86

endif

# Windows
ifdef HAVE_WIN32
ifndef HAVE_MINGW_W64
DEPS_ffmpeg += directx
endif
FFMPEGCONF += --target-os=mingw32 --enable-memalign-hack
FFMPEGCONF += --enable-w32threads --enable-dxva2 \
	--disable-decoder=dca

ifdef HAVE_WIN64
FFMPEGCONF += --cpu=athlon64 --arch=x86_64
else # !WIN64
FFMPEGCONF+= --cpu=i686 --arch=x86
endif

else # !Windows
FFMPEGCONF += --enable-pthreads
endif

# Build
PKGS += ffmpeg
ifeq ($(call need_pkg,"libavcodec >= 52.25.0 libavformat >= 52.30.0 libswscale"),)
PKGS_FOUND += ffmpeg
endif

$(TARBALLS)/ffmpeg-git.tar.gz:
	$(call download,$(FFMPEG_SNAPURL))

FFMPEG_VERSION := git

.sum-ffmpeg: $(TARBALLS)/ffmpeg-$(FFMPEG_VERSION).tar.gz
	$(warning Not implemented.)
	touch $@

ffmpeg: ffmpeg-$(FFMPEG_VERSION).tar.gz .sum-ffmpeg
	rm -Rf $@ $@-git
	mkdir -p $@-git
	$(ZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(MOVE)

.ffmpeg: ffmpeg
	cd $< && $(HOSTVARS) ./configure \
		--extra-ldflags="$(LDFLAGS)" $(FFMPEGCONF) \
		--prefix="$(PREFIX)" --enable-static --disable-shared
	cd $< && $(MAKE) install-libs install-headers
	touch $@
