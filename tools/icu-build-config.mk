# Shared target/host ICU selection. Include after CROSS_PREFIX is resolved.
# ICU 74 is the API floor. ICU 78.3 remains the validated data baseline.
PKG_CONFIG?=pkg-config
HOST_PKG_CONFIG?=pkg-config
ICU_STATIC?=n
ICU_PKG_MODULES?=icu-i18n icu-uc

ifeq ($(CONFIG_ICU),y)
# --static expands private dependencies; explicit archive paths or linker
# selectors are needed to require archive linking when .so files also exist.
ICU_PKG_LINK_MODE=
ifeq ($(ICU_STATIC),y)
ICU_PKG_LINK_MODE=--static
endif

ifeq ($(origin ICU_LIBS),undefined)
ifneq ($(CROSS_PREFIX),)
ifeq ($(origin PKG_CONFIG),file)
ifeq ($(strip $(PKG_CONFIG_LIBDIR)$(PKG_CONFIG_SYSROOT_DIR)),)
$(error ICU cross builds need target ICU_CFLAGS and ICU_LIBS or an explicitly target-scoped PKG_CONFIG/PKG_CONFIG_LIBDIR)
endif
endif
endif
ifeq ($(shell $(PKG_CONFIG) --atleast-version=74 $(ICU_PKG_MODULES) >/dev/null 2>&1 && echo y),)
$(error CONFIG_ICU=y requires ICU4C >=74 development APIs; select a supported system ICU with PKG_CONFIG or provide matching ICU_CFLAGS and ICU_LIBS)
endif
ICU_LIBS:=$(shell $(PKG_CONFIG) $(ICU_PKG_LINK_MODE) --libs $(ICU_PKG_MODULES))
endif
ifeq ($(strip $(ICU_LIBS)),)
$(error CONFIG_ICU=y requires nonempty ICU_LIBS; ICU discovery returned no target libraries)
endif
ifeq ($(origin ICU_CFLAGS),undefined)
ifeq ($(shell $(PKG_CONFIG) --atleast-version=74 $(ICU_PKG_MODULES) >/dev/null 2>&1 && echo y),)
$(error Automatic ICU_CFLAGS requires ICU4C >=74 pkg-config metadata; provide ICU_CFLAGS matching the selected ICU_LIBS)
endif
ICU_CFLAGS:=$(shell $(PKG_CONFIG) --cflags $(ICU_PKG_MODULES))
endif

ifneq ($(CROSS_PREFIX),)
# A target sysroot exported for pkg-config must not select host-qjsc's ICU.
ifneq ($(strip $(PKG_CONFIG_LIBDIR)$(PKG_CONFIG_SYSROOT_DIR)),)
ifeq ($(origin HOST_PKG_CONFIG),file)
ifeq ($(origin HOST_ICU_LIBS),undefined)
$(error ICU cross builds with target pkg-config environment need an explicitly host-scoped HOST_PKG_CONFIG or HOST_ICU_CFLAGS and HOST_ICU_LIBS)
endif
ifeq ($(origin HOST_ICU_CFLAGS),undefined)
$(error ICU cross builds with target pkg-config environment need explicit HOST_ICU_CFLAGS or an explicitly host-scoped HOST_PKG_CONFIG)
endif
endif
endif
ifeq ($(origin HOST_ICU_LIBS),undefined)
ifeq ($(shell $(HOST_PKG_CONFIG) --atleast-version=74 $(ICU_PKG_MODULES) >/dev/null 2>&1 && echo y),)
$(error ICU-enabled host-qjsc requires host ICU4C >=74 development APIs; select HOST_PKG_CONFIG or matching HOST_ICU_CFLAGS and HOST_ICU_LIBS)
endif
HOST_ICU_LIBS:=$(shell $(HOST_PKG_CONFIG) $(ICU_PKG_LINK_MODE) --libs $(ICU_PKG_MODULES))
endif
ifeq ($(origin HOST_ICU_CFLAGS),undefined)
ifeq ($(shell $(HOST_PKG_CONFIG) --atleast-version=74 $(ICU_PKG_MODULES) >/dev/null 2>&1 && echo y),)
$(error Automatic HOST_ICU_CFLAGS requires host ICU4C >=74 pkg-config metadata; provide flags matching HOST_ICU_LIBS)
endif
HOST_ICU_CFLAGS:=$(shell $(HOST_PKG_CONFIG) --cflags $(ICU_PKG_MODULES))
endif
ifeq ($(strip $(HOST_ICU_LIBS)),)
$(error ICU-enabled cross builds need nonempty host ICU libraries)
endif
else
HOST_ICU_LIBS?=$(ICU_LIBS)
HOST_ICU_CFLAGS?=$(ICU_CFLAGS)
endif
endif
