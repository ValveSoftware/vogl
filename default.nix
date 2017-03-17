# -*- indent-tabs-mode: nil -*-
{ fetchgit, stdenv
, cmake, git, pkgconfig, wget, zip
, makeQtWrapper, qtbase, qtx11extras
, libdwarf, libjpeg_turbo, libunwind, lzma, tinyxml, libX11
, SDL2, SDL2_gfx, SDL2_image, SDL2_ttf
, freeglut, mesa, mesa_glu
##
, localSource ? false
}:
let
  ver = "2016-05-13";
in
stdenv.mkDerivation {
  name = "vogl-${ver}";
  version = "${ver}";

  src = fetchgit {
	  url    = if localSource == false then "https://github.com/deepfire/vogl.git" else ("file://" + localSource);
          rev    = "cbc5f1853e294b363f16c4e00b3e0c49dbf74559";
          sha256 = "17gwd73x3lnqv6ccqs48pzqwbzjhbn41c0x0l5zzirhiirb3yh0n";
        };

  nativeBuildInputs = [
    makeQtWrapper pkgconfig
  ];

  buildInputs = [
    cmake git pkgconfig wget zip
    qtbase qtx11extras
    libdwarf libjpeg_turbo libunwind lzma tinyxml libX11
    SDL2 SDL2_gfx SDL2_image SDL2_ttf
    freeglut mesa mesa_glu
  ];

  enableParallelBuilding = true;

  dontUseCmakeBuildDir = true;
  preConfigure = ''
    cmakeDir=$PWD
    mkdir -p vogl/vogl_build/release64 && cd $_  
  '';
  cmakeFlags = '' -DCMAKE_VERBOSE=On -DCMAKE_BUILD_TYPE=Release -DBUILD_X64=On'';

  meta = with stdenv.lib; {
    description = "OpenGL capture / playback debugger.";
    homepage = https://github.com/ValveSoftware/vogl;
    license = licenses.bsd3;
    maintainers = with maintainers; [ deepfire ];
    platforms = platforms.all;
  };
}
