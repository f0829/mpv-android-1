# somewhat inspired by vitasdk

cmake_minimum_required(VERSION 3.2.0)

set(arch arm)

set(FREETYPE2_VERSION 2.8.1)
set(FREETYPE2_HASH SHA256=876711d064a6a1bd74beb18dd37f219af26100f72daaebd2d86cb493d7cd7ec6)

set(FRIBIDI_VERSION 0.19.7)
set(FRIBIDI_HASH SHA256=08222a6212bbc2276a2d55c3bf370109ae4a35b689acbc66571ad2a670595a8e)

set(LIBASS_VERSION 0.14.0)
set(LIBASS_HASH SHA256=8d5a5c920b90b70a108007ffcd2289ac652c0e03fc88e6eecefa37df0f2e7fdf)

set(LUA_VERSION 5.2.4)
set(LUA_HASH SHA256=b9e2e4aad6789b3b63a056d442f7b39f0ecfca3ae0f1fc0ae4e9614401b69f4b)

set(NETTLE_VERSION 3.4)
set(NETTLE_HASH SHA256=ae7a42df026550b85daca8389b6a60ba6313b0567f374392e54918588a411e94)

set(GNUTLS_VERSION 3.6.1)
set(GNUTLS_HASH SHA256=20b10d2c9994bc032824314714d0e84c0f19bdb3d715d8ed55beb7364a8ebaed)

################## END OF CONFIGURABLE OPTIONS ##################

if(arch STREQUAL "arm")
    set(ndk_triple arm-linux-androideabi)
    set(cpu armv7-a)
else()
    message(FATAL_ERROR "Building for unknown architecture ${arch}.")
endif()

set(CMAKE_C_COMPILER ${ndk_triple}-clang)
set(CMAKE_CXX_COMPILER ${ndk_triple}-clang++)

project(mpv-android-build)

if("${CMAKE_VERSION}" VERSION_GREATER 3.6.0)
    set(GIT_SHALLOW_SUPPORT GIT_SHALLOW 1)
endif()

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/prefix/${arch})

include(ExternalProject)
set(wrapper_command ${CMAKE_SOURCE_DIR}/command_wrapper.sh)

set(compiler_flags "CC=${CMAKE_C_COMPILER}")

message(STATUS "Using NDK triple: ${ndk_triple}")

ExternalProject_Add(freetype2
    URL https://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE2_VERSION}.tar.gz
    URL_HASH ${FREETYPE2_HASH}

    CONFIGURE_COMMAND PKG_CONFIG=/bin/false ${compiler_flags} ${wrapper_command} <SOURCE_DIR>/configure
    --host=${ndk_triple}
    --prefix=${CMAKE_INSTALL_PREFIX}
    --enable-static
    --disable-shared
    --with-png=no

    BUILD_COMMAND ${wrapper_command} $(MAKE)

    INSTALL_COMMAND $(MAKE) install
)

ExternalProject_Add(fribidi
    URL https://download.videolan.org/contrib/fribidi/fribidi-${FRIBIDI_VERSION}.tar.bz2
    URL_HASH ${FRIBIDI_HASH}

    CONFIGURE_COMMAND PKG_CONFIG=/bin/false ${compiler_flags} ${wrapper_command} <SOURCE_DIR>/configure
    --host=${ndk_triple}
    --prefix=${CMAKE_INSTALL_PREFIX}
    --enable-static
    --disable-shared

    BUILD_COMMAND ${wrapper_command} $(MAKE)

    INSTALL_COMMAND $(MAKE) install
)

ExternalProject_Add(libass
    DEPENDS freetype2 fribidi

    URL https://github.com/libass/libass/releases/download/${LIBASS_VERSION}/libass-${LIBASS_VERSION}.tar.gz
    URL_HASH ${LIBASS_HASH}

    CONFIGURE_COMMAND ${compiler_flags} ${wrapper_command} <SOURCE_DIR>/configure
    --host=${ndk_triple}
    --prefix=${CMAKE_INSTALL_PREFIX}
    --enable-static
    --disable-shared
    --disable-require-system-font-provider

    BUILD_COMMAND ${wrapper_command} $(MAKE)

    INSTALL_COMMAND $(MAKE) install
)

ExternalProject_Add(lua
    URL http://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz
    URL_HASH ${LUA_HASH}

    CONFIGURE_COMMAND true

    BUILD_IN_SOURCE 1

    BUILD_COMMAND ${wrapper_command} $(MAKE) 'CC="${CMAKE_C_COMPILER} -Dgetlocaledecpoint\(\)=\(\'.\'\)"'
    'AR="${ndk_triple}-ar r"'
    RANLIB="${ndk_triple}-ranlib"
    PLAT=linux LUA_T= LUAC_T=

    INSTALL_COMMAND $(MAKE) INSTALL_TOP="${CMAKE_INSTALL_PREFIX}" TO_BIN=/dev/null install
    COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/
    COMMAND make pc > ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/lua.pc
    COMMAND cat ${CMAKE_SOURCE_DIR}/patches/lua-extra-pc.txt >> ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/lua.pc
)

ExternalProject_Add(nettle
    URL https://ftp.gnu.org/gnu/nettle/nettle-${NETTLE_VERSION}.tar.gz
    URL_HASH ${NETTLE_HASH}

    CONFIGURE_COMMAND ${compiler_flags} ${wrapper_command} <SOURCE_DIR>/configure
    --host=${ndk_triple}
    --prefix=${CMAKE_INSTALL_PREFIX}
    --enable-mini-gmp
    --enable-static
    --disable-shared

    BUILD_COMMAND ${wrapper_command} $(MAKE)

    INSTALL_COMMAND $(MAKE) install
    COMMAND ln -sf libhogweed.a ${CMAKE_INSTALL_PREFIX}/lib/libgmp.a
    COMMAND cp ${CMAKE_SOURCE_DIR}/patches/gmp.h ${CMAKE_INSTALL_PREFIX}/include/gmp.h
)

# TODO: dynamic 3.6
ExternalProject_Add(gnutls
    DEPENDS nettle

    URL https://www.gnupg.org/ftp/gcrypt/gnutls/v3.6/gnutls-${GNUTLS_VERSION}.tar.xz
    URL_HASH ${GNUTLS_HASH}

    CONFIGURE_COMMAND ${compiler_flags} ${wrapper_command} <SOURCE_DIR>/configure
    --host=${ndk_triple}
    --prefix=${CMAKE_INSTALL_PREFIX}
    --enable-static
    --disable-shared
    --with-nettle-mini
    --with-included-libtasn1
    --with-included-unistring
    --disable-doc
    --disable-tools
    --disable-cxx
    --disable-tests
    --without-p11-kit

    BUILD_COMMAND ${wrapper_command} $(MAKE)

    INSTALL_COMMAND $(MAKE) install
)

ExternalProject_Add(ffmpeg
    DEPENDS gnutls

    GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
    ${GIT_SHALLOW_SUPPORT}

    CONFIGURE_COMMAND ${compiler_flags} ${wrapper_command} <SOURCE_DIR>/configure
    --prefix=${CMAKE_INSTALL_PREFIX}
    --target-os=android
    --enable-cross-compile
    --cross-prefix=${ndk_triple}-
    --cc=${CMAKE_C_COMPILER}
    --arch=${arch}
    --cpu=${cpu}
    --enable-jni
    --enable-mediacodec
    --enable-gmp
    --enable-gnutls
    --extra-cflags="-I${CMAKE_INSTALL_PREFIX}/include"
    --extra-ldflags="-L${CMAKE_INSTALL_PREFIX}/lib"
    --disable-static
    --enable-shared
    --enable-version3
    --pkg-config=pkg-config
    --pkg-config-flags=--static
    --disable-debug
    --disable-doc

    BUILD_COMMAND ${wrapper_command} $(MAKE)

    INSTALL_COMMAND $(MAKE) install
)
