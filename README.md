Directory Structure :
Backend Communication
  |--CmakeList.txt (Main cmake)
     |--Application
        |--CmakeLists.txt for application
     |--src
        |--CmakeLists.txt for unitcomm
     |--include(Hearer file)
     |--Build (cmake intermediate file)
     |--bin(Generated binaries)

Steps to compile the source code 

1) source /opt/connexus-agl/6.0.2/environment-setup-cortexa8hf-neon-agl-linux-gnueabi
2) mkdir Build
3) cd Build
4) cmake ..
5) make all
6) make install (will install unitcomm binaries in bin folder)
