#! /bin/bash
#
cdir=$(pwd)
clear
printf "DSD-FME Digital Speech Decoder - Florida Man Edition Portable Cygwin Creator\n"
printf "Only use this in a Cygwin64 Environment. Will not work in a Linux Environment!\n"
read -p "Do you want to make a portable copy? y/N " RELEASE
RELEASE=$(printf "$RELEASE"|tr '[:upper:]' '[:lower:]')
if [ "$RELEASE" = "y" ]; then

  printf "Copying files to dsd-fme-portable folder and making zip file. Please wait!\n"
  cd ~

  #create a dsd-fme-portable folder and copy all the needed items for a portable release version
  mkdir dsd-fme-portable
  cd dsd-fme-portable
  #all of these folders may not be necessary
  mkdir sourcecode
  mkdir bin
  mkdir dev
  cd dev
  mkdir shm
  cd ..
  mkdir dsd-fme
  mkdir logs
  mkdir WAV
  mkdir etc
  mkdir home
  mkdir lib
  mkdir tmp
  mkdir usr
  cd usr
  mkdir share
  cd share
  mkdir terminfo
  cd ~

  #start copying things

  #terminfo is needed for ncurses
  cp -r /usr/share/terminfo/* dsd-fme-portable/usr/share/terminfo

  #copy required items from the root bin folder to release bin folder
  cp /bin/ls.exe dsd-fme-portable/bin/
  cp /bin/bash.exe dsd-fme-portable/bin/
  cp /bin/ncurses6-config dsd-fme-portable/bin/
  cp /bin/ncursesw6-config dsd-fme-portable/bin/
  cp /bin/pacat.exe dsd-fme-portable/bin/
  cp /bin/pactl.exe dsd-fme-portable/bin/
  cp /bin/padsp dsd-fme-portable/bin/
  cp /bin/pamon dsd-fme-portable/bin/
  cp /bin/paplay dsd-fme-portable/bin/
  cp /bin/parec dsd-fme-portable/bin/
  cp /bin/parecord dsd-fme-portable/bin/
  cp /bin/pulseaudio.exe dsd-fme-portable/bin/
  cp /bin/tty.exe dsd-fme-portable/bin/

  #start copying things from the etc folder
  cp -r /etc/pulse/ dsd-fme-portable/etc/

  #start copying things to the lib folder
  cp -r /lib/pulseaudio/ dsd-fme-portable/lib/

  #start copying things into the dsd-fme folder
  cp dsd-fme/build/dsd-fme.exe dsd-fme-portable/dsd-fme/
  cp -r dsd-fme/examples dsd-fme-portable/dsd-fme/
  cp .bash_profile dsd-fme-portable/dsd-fme/
  cp .bashrc dsd-fme-portable/dsd-fme/
  cp .inputrc dsd-fme-portable/dsd-fme/
  cp .profile dsd-fme-portable/dsd-fme/

  #move (cut) the bat files to the portable folder root and delete the other ones so users won't get confused
  mv dsd-fme-portable/dsd-fme/examples/cygwin_bat/*.bat dsd-fme-portable/
  mv dsd-fme-portable/dsd-fme/examples/cygwin_bat/example_options.txt dsd-fme-portable/
  mv dsd-fme-portable/dsd-fme/examples/cygwin_bat/complete_usage_options.txt dsd-fme-portable/
  rm -rf dsd-fme-portable/dsd-fme/examples/cygwin_bat

  #change .bat files permissions to be read/write/executable by all users (for some reason, this is not preserved)
  chmod 777 dsd-fme-portable/*.bat

  #copy the license, copyright, source code, and things that should be included in all portable releases
  cp dsd-fme/COPYRIGHT dsd-fme-portable/sourcecode/
  cp dsd-fme/DSD_Author.pgp dsd-fme-portable/sourcecode/
  cp dsd-fme/dsd-fme-qgis-map.qgz dsd-fme-portable/sourcecode/
  cp dsd-fme/README.md dsd-fme-portable/sourcecode/
  cp dsd-fme/dsd-fme2.png dsd-fme-portable/sourcecode/
  cp dsd-fme/dsd-fme3.png dsd-fme-portable/sourcecode/

  cp dsd-fme/CMakeLists.txt dsd-fme-portable/sourcecode/
  cp -r dsd-fme/src dsd-fme-portable/sourcecode/
  cp -r dsd-fme/include dsd-fme-portable/sourcecode/
  cp -r dsd-fme/cmake dsd-fme-portable/sourcecode/
  cp -r dsd-fme/patch dsd-fme-portable/sourcecode/

  #compiled items into release dsd-fme folder
  cp codec2/build/src/cygcodec2* dsd-fme-portable/dsd-fme/
  cp rtl-sdr/build/src/cyg* dsd-fme-portable/dsd-fme/
  cp mbelib/build/cygmbe* dsd-fme-portable/dsd-fme/

  #bin items into release dsd-fme folder (uisng * where version numbering may exist, those may be upgraded if hard coded)
  cp /bin/bash.exe dsd-fme-portable/dsd-fme/
  cp /bin/cut.exe dsd-fme-portable/dsd-fme/
  cp /bin/cygao* dsd-fme-portable/dsd-fme/
  cp /bin/cygarchive* dsd-fme-portable/dsd-fme/
  cp /bin/cygargp* dsd-fme-portable/dsd-fme/
  cp /bin/cygassuan* dsd-fme-portable/dsd-fme/
  cp /bin/cygasyncns* dsd-fme-portable/dsd-fme/
  cp /bin/cygatomic* dsd-fme-portable/dsd-fme/
  cp /bin/cygattr* dsd-fme-portable/dsd-fme/
  cp /bin/cygavahi-client* dsd-fme-portable/dsd-fme/
  cp /bin/cygavahi-common* dsd-fme-portable/dsd-fme/
  cp /bin/cygavahi-wrap* dsd-fme-portable/dsd-fme/
  cp /bin/cygblkid* dsd-fme-portable/dsd-fme/
  cp /bin/cygbrotlicommon* dsd-fme-portable/dsd-fme/
  cp /bin/cygbrotlidec* dsd-fme-portable/dsd-fme/
  cp /bin/cygbz2* dsd-fme-portable/dsd-fme/
  cp /bin/cygcares* dsd-fme-portable/dsd-fme/
  cp /bin/cygcheck.exe dsd-fme-portable/dsd-fme/
  cp /bin/cygcli.dll dsd-fme-portable/dsd-fme/
  cp /bin/cygcom_err* dsd-fme-portable/dsd-fme/
  cp /bin/cygcrypt* dsd-fme-portable/dsd-fme/
  cp /bin/cygcrypto* dsd-fme-portable/dsd-fme/
  cp /bin/cygcurl* dsd-fme-portable/dsd-fme/
  cp /bin/cygdb_cxx* dsd-fme-portable/dsd-fme/
  cp /bin/cygdb_sql* dsd-fme-portable/dsd-fme/
  cp /bin/cygdb* dsd-fme-portable/dsd-fme/
  cp /bin/cygdbus* dsd-fme-portable/dsd-fme/
  cp /bin/cygedit* dsd-fme-portable/dsd-fme/
  cp /bin/cygexpat* dsd-fme-portable/dsd-fme/
  cp /bin/cygfdisk* dsd-fme-portable/dsd-fme/
  cp /bin/cygffi* dsd-fme-portable/dsd-fme/
  cp /bin/cygfftw3_threads* dsd-fme-portable/dsd-fme/
  cp /bin/cygfftw3* dsd-fme-portable/dsd-fme/
  cp /bin/cygfftw3f_threads* dsd-fme-portable/dsd-fme/
  cp /bin/cygfftw3f* dsd-fme-portable/dsd-fme/
  cp /bin/cygfftw3l_threads* dsd-fme-portable/dsd-fme/
  cp /bin/cygfftw3l* dsd-fme-portable/dsd-fme/
  cp /bin/cygfido2* dsd-fme-portable/dsd-fme/
  cp /bin/cygFLAC* dsd-fme-portable/dsd-fme/
  cp /bin/cygformw* dsd-fme-portable/dsd-fme/
  cp /bin/cyggc* dsd-fme-portable/dsd-fme/
  cp /bin/cyggdbm_compat* dsd-fme-portable/dsd-fme/
  cp /bin/cyggdbm*.dll dsd-fme-portable/dsd-fme/
  cp /bin/cyggio* dsd-fme-portable/dsd-fme/
  cp /bin/cygglib* dsd-fme-portable/dsd-fme/
  cp /bin/cyggmodule* dsd-fme-portable/dsd-fme/
  cp /bin/cyggmp* dsd-fme-portable/dsd-fme/
  cp /bin/cyggnutls* dsd-fme-portable/dsd-fme/
  cp /bin/cyggobject* dsd-fme-portable/dsd-fme/
  cp /bin/cyggomp* dsd-fme-portable/dsd-fme/
  cp /bin/cyggpg-error* dsd-fme-portable/dsd-fme/
  cp /bin/cyggpgme* dsd-fme-portable/dsd-fme/
  cp /bin/cyggsm* dsd-fme-portable/dsd-fme/
  cp /bin/cyggssapi_krb5* dsd-fme-portable/dsd-fme/
  cp /bin/cyggthread* dsd-fme-portable/dsd-fme/
  cp /bin/cygguile* dsd-fme-portable/dsd-fme/
  cp /bin/cyghistory* dsd-fme-portable/dsd-fme/
  cp /bin/cyghogweed* dsd-fme-portable/dsd-fme/
  cp /bin/cygICE* dsd-fme-portable/dsd-fme/
  cp /bin/cygiconv* dsd-fme-portable/dsd-fme/
  cp /bin/cygid3tag* dsd-fme-portable/dsd-fme/
  cp /bin/cygidn* dsd-fme-portable/dsd-fme/
  cp /bin/cygintl* dsd-fme-portable/dsd-fme/
  cp /bin/cygisl* dsd-fme-portable/dsd-fme/
  cp /bin/cygjsoncpp* dsd-fme-portable/dsd-fme/
  cp /bin/cygk5crypto* dsd-fme-portable/dsd-fme/
  cp /bin/cygkrb5* dsd-fme-portable/dsd-fme/
  cp /bin/cyglber* dsd-fme-portable/dsd-fme/
  cp /bin/cygldap* dsd-fme-portable/dsd-fme/
  cp /bin/cygltdl* dsd-fme-portable/dsd-fme/
  cp /bin/cyglz* dsd-fme-portable/dsd-fme/
  cp /bin/cygma* dsd-fme-portable/dsd-fme/
  cp /bin/cygmenuw* dsd-fme-portable/dsd-fme/
  cp /bin/cygmeta* dsd-fme-portable/dsd-fme/
  cp /bin/cygmp* dsd-fme-portable/dsd-fme/
  cp /bin/cygncurses* dsd-fme-portable/dsd-fme/
  cp /bin/cygnettle* dsd-fme-portable/dsd-fme/
  cp /bin/cygnghttp* dsd-fme-portable/dsd-fme/
  cp /bin/cygobjc* dsd-fme-portable/dsd-fme/
  cp /bin/cygogg* dsd-fme-portable/dsd-fme/
  cp /bin/cygopus* dsd-fme-portable/dsd-fme/
  cp /bin/cygorc* dsd-fme-portable/dsd-fme/
  cp /bin/cygp11* dsd-fme-portable/dsd-fme/
  cp /bin/cygpanel* dsd-fme-portable/dsd-fme/
  cp /bin/cygpath.exe dsd-fme-portable/dsd-fme/
  cp /bin/cygpcre* dsd-fme-portable/dsd-fme/
  cp /bin/cygperl* dsd-fme-portable/dsd-fme/
  cp /bin/cygpipe* dsd-fme-portable/dsd-fme/
  cp /bin/cygpkg* dsd-fme-portable/dsd-fme/
  cp /bin/cygpng* dsd-fme-portable/dsd-fme/
  cp /bin/cygpopt* dsd-fme-portable/dsd-fme/
  cp /bin/cygprotocol* dsd-fme-portable/dsd-fme/
  cp /bin/cygpsl* dsd-fme-portable/dsd-fme/
  cp /bin/cygpulse* dsd-fme-portable/dsd-fme/
  cp /bin/cygquadmath* dsd-fme-portable/dsd-fme/
  cp /bin/cygreadline* dsd-fme-portable/dsd-fme/
  cp /bin/cygrhash* dsd-fme-portable/dsd-fme/
  cp /bin/cygrunsrv.exe dsd-fme-portable/dsd-fme/
  cp /bin/cygsamplerate* dsd-fme-portable/dsd-fme/
  cp /bin/cygsasl* dsd-fme-portable/dsd-fme/
  cp /bin/cygserver* dsd-fme-portable/dsd-fme/
  cp /bin/cygSM* dsd-fme-portable/dsd-fme/
  cp /bin/cygsmart* dsd-fme-portable/dsd-fme/
  cp /bin/cygsndfile* dsd-fme-portable/dsd-fme/
  cp /bin/cygsox* dsd-fme-portable/dsd-fme/
  cp /bin/cygspeex* dsd-fme-portable/dsd-fme/
  cp /bin/cygsqlite* dsd-fme-portable/dsd-fme/
  cp /bin/cygssh* dsd-fme-portable/dsd-fme/
  cp /bin/cygssl* dsd-fme-portable/dsd-fme/
  cp /bin/cygstart.exe dsd-fme-portable/dsd-fme/
  cp /bin/cygstdc* dsd-fme-portable/dsd-fme/
  cp /bin/cygtasn* dsd-fme-portable/dsd-fme/
  cp /bin/cygtdb* dsd-fme-portable/dsd-fme/
  cp /bin/cygticw* dsd-fme-portable/dsd-fme/
  cp /bin/cygtwo* dsd-fme-portable/dsd-fme/
  cp /bin/cygunistring* dsd-fme-portable/dsd-fme/
  cp /bin/cygusb* dsd-fme-portable/dsd-fme/
  cp /bin/cyguuid* dsd-fme-portable/dsd-fme/
  cp /bin/cyguv* dsd-fme-portable/dsd-fme/
  cp /bin/cygvorbis* dsd-fme-portable/dsd-fme/
  cp /bin/cygwavpack* dsd-fme-portable/dsd-fme/
  cp /bin/cygwin1.dll dsd-fme-portable/dsd-fme/
  cp /bin/cygwrap* dsd-fme-portable/dsd-fme/
  cp /bin/cygX* dsd-fme-portable/dsd-fme/
  cp /bin/cygx* dsd-fme-portable/dsd-fme/
  cp /bin/cygz* dsd-fme-portable/dsd-fme/
  cp /bin/grep.exe dsd-fme-portable/dsd-fme/
  cp /bin/ls.exe dsd-fme-portable/dsd-fme/
  cp /bin/mintty.exe dsd-fme-portable/dsd-fme/
  cp /bin/pacat.exe dsd-fme-portable/dsd-fme/
  cp /bin/pacmd.exe dsd-fme-portable/dsd-fme/
  cp /bin/pactl.exe dsd-fme-portable/dsd-fme/
  cp /bin/padsp dsd-fme-portable/dsd-fme/
  cp /bin/pamon dsd-fme-portable/dsd-fme/
  cp /bin/paplay dsd-fme-portable/dsd-fme/
  cp /bin/parec dsd-fme-portable/dsd-fme/
  cp /bin/pulseaudio.exe dsd-fme-portable/dsd-fme/
  cp /bin/tail.exe dsd-fme-portable/dsd-fme/
  cp /bin/touch.exe dsd-fme-portable/dsd-fme/
  cp /bin/tty.exe dsd-fme-portable/dsd-fme/

  zip -r dsd-fme-x86-64-cygwin-portable.zip dsd-fme-portable

fi
