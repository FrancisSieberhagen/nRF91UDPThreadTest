# nRF91UDPThreadTest

## Test BSD library - NB-IoT UDP Thread client send JSON data to server (server/udp_server)

### nRF Connect SDK!
    https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html

    Installing the nRF Connect SDK through nRF Connect for Desktop
    https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/gs_assistant.html

### Nordicsemi nRF9160 NB-IoT 
    https://www.nordicsemi.com/Software-and-tools/Development-Kits/nRF9160-DK

### CLion - A cross-platform IDE for C
    https://devzone.nordicsemi.com/f/nordic-q-a/49730/howto-nrf9160-development-with-clion

### Application Description
    JSON Data packet {"ActionName":"BSD Test","LED1":false,"LED2":true}

    Client: Send JSON packet to server
    Server: Send JSON packet to client

    Client: Validate ActionName - "ActionName":"BSD Test"
    Client: Toggle nRF9160-DK leds - "LED1":false,"LED2":true

### Change Test Server ip & port in prj.conf  
    CONFIG_SERVER_HOST="139.162.251.115"
    CONFIG_SERVER_PORT=42511

### Build hex 
    $ export ZEPHYR_BASE=/????
    $ west build -b nrf9160_pca10090ns

### Program nRF9160-DK using nrfjprog
    $ nrfjprog --program build/zephyr/merged.hex -f nrf91 --chiperase --reset --verify


### X86 QEMU board 
    X86 QEMU board configuration is used to emulate the X86 architecture
    http://developer.nordicsemi.com/nRF_Connect_SDK/doc/1.4.2/zephyr/boards/x86/qemu_x86/doc/index.html#qemu-x86

    SLIP networking over an emulated serial port (CONFIG_NET_SLIP_TAP=y). The detailed setup is described
    http://developer.nordicsemi.com/nRF_Connect_SDK/doc/1.4.2/zephyr/guides/networking/qemu_setup.html#networking-with-qemu

    $ export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
    $ export ZEPHYR_SDK_INSTALL_DIR=??/zephyr-sdk-0.??.?

    $ west build -b qemu_x86 
    $ west build -t run

### Build server
    $ gcc udp_server.c -o udp_server


### nRF Connect
![alt text](https://raw.githubusercontent.com/FrancisSieberhagen/nRF91UDPThreadTest/master/images/nRFConnect.jpg)


### Server
![alt text](https://raw.githubusercontent.com/FrancisSieberhagen/nRF91UDPThreadTest/master/images/server.jpg)



