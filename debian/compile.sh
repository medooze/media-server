!/bin/sh

#Get number of processors available
PROCS=´nproc´

# Install devel libraries in Debian/Ubuntu
#
apt-get -y install libgsm1-dev g++ make libtool subversion git automake subversion autoconf libgcrypt11-dev libjpeg8-dev libssl-dev
 
echo "/usr/local/lib" > /etc/ld.so.conf.d/local.conf
ldconfig
 
#
# Needed by webrtc 
#
apt-get -y install bzip2 pkg-config libgtk2.0-dev libnss3-dev libxtst-dev libxss-dev libdbus-1-dev libdrm-dev libgconf2-dev libgnome-keyring-dev libpci-dev libudev-dev 
 
#
# External source code checkout
#
mkdir -p /usr/local/src
cd /usr/local/src
 
wget http://downloads.sourceforge.net/project/xmlrpc-c/Xmlrpc-c%20Super%20Stable/1.16.35/xmlrpc-c-1.16.35.tgz
tar xvzf xmlrpc-c-1.16.35.tgz
wget http://downloads.xiph.org/releases/speex/speex-1.2rc1.tar.gz
tar xvzf speex-1.2rc1.tar.gz
wget http://downloads.xiph.org/releases/opus/opus-1.0.2.tar.gz
tar xvzf opus-1.0.2.tar.gz
wget http://www.tortall.net/projects/yasm/releases/yasm-1.2.0.tar.gz
tar xvzf yasm-1.2.0.tar.gz
 
svn checkout svn://svn.code.sf.net/p/mcumediaserver/code/trunk medooze
svn checkout http://mp4v2.googlecode.com/svn/trunk/ mp4v2
git clone git://git.videolan.org/ffmpeg.git
git clone git://git.videolan.org/x264.git
git clone https://github.com/webmproject/libvpx/
git clone https://github.com/cisco/libsrtp

svn co http://src.chromium.org/chrome/trunk/tools/depot_tools
export PATH="$PATH":/usr/local/src/depot_tools


#
# Compiling yasm 1.2
#
cd yasm-1.2.0
./configure
make -j ${PROCS}
make install
cd ..
 
#
# Compiling X264
#
 
cd /usr/local/src/x264
./configure --enable-debug --enable-shared --enable-pic
make -j ${PROCS}
make install
cd ..

#
# Compiling FFMPEG
#
 
cd /usr/local/src/ffmpeg
./configure --enable-shared --enable-gpl --enable-nonfree --disable-stripping --enable-zlib --enable-avresample --enable-decoder=png
make -j ${PROCS}
make install
cd ..

#
# Compiling XMLRPC-C
#
 
cd /usr/local/src/xmlrpc-c-1.16.35
./configure
make -j ${PROCS}
make install
cd ..

#
# Compiling mp4v2
#
 
cd /usr/local/src/mp4v2
autoreconf -fiv
./configure
make -j ${PROCS}
make install

#
# Compiling Speex
#
 
cd /usr/local/src/libvpx
./configure --enable-shared
make -j ${PROCS}
make install

#
# Compiling Speex
#
 
cd /usr/local/src/speex-1.2rc1
./configure
make -j ${PROCS}
make install

#
# Compiling Opus
#
 
cd /usr/local/src/opus-1.0.2
./configure
make -j ${PROCS}
make install


#
# Compiling libsrtp
#
 
cd /usr/local/src/libsrtp
./configure
make -j ${PROCS}
make install


#
# Compile WebRTC VAD and signal processing libraries
#
cd /usr/local/src/medooze/mcu/ext
ninja -C out/Release/ common_audio

