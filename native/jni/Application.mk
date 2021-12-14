APP_ABI          := armeabi-v7a arm64-v8a x86 x86_64
APP_CFLAGS       := -Wall -Oz -fomit-frame-pointer -flto
APP_LDFLAGS      := -flto
APP_CPPFLAGS     := -std=c++17
APP_STL          := none
APP_PLATFORM     := android-21
APP_THIN_ARCHIVE := true
APP_STRIP_MODE   := --strip-all

ifneq ($(TARGET_ARCH),arm64)
ifneq ($(TARGET_ARCH),x86_64)
ifndef B_SHARED
# Disable fortify on static 32-bit targets
APP_CFLAGS       += -D_FORTIFY_SOURCE=0 -Wno-macro-redefined
endif
endif
endif

# Busybox should use stock libc.a
ifdef B_BB
APP_PLATFORM     := android-22
ifeq ($(OS),Windows_NT)
APP_SHORT_COMMANDS := true
endif
endif
