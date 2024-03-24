{ lib
, stdenv
, cmake
, mbelib
, libsndfile
, ncurses
, pulseaudio
, rtl-sdr
, itpp
, codec2
, portaudioSupport ? true
, portaudio ? null
}:

assert portaudioSupport -> portaudio != null;

stdenv.mkDerivation {
  name = "dsd-fme";

  src = ./.;

  nativeBuildInputs = [ cmake ];
  buildInputs = [
    mbelib
    libsndfile
    itpp
    rtl-sdr
    ncurses.dev
    pulseaudio.dev
    codec2
  ] ++ lib.optionals portaudioSupport [ portaudio ];

  doCheck = true;

  meta = with lib; {
    description = "Digital Speech Decoder - Florida Man Edition";
    longDescription = ''
      DSD is able to decode several digital voice formats from discriminator
      tap audio and synthesize the decoded speech. Speech synthesis requires
      mbelib, which is a separate package.
    '';
    homepage = "https://github.com/szechyjs/dsd";
    license = licenses.mit;
    platforms = platforms.unix;
  };
}
