# SMA Inverter Node.js SDK

## Purpose

The purpose of this project is to provide a complete bridge between SMA solar inverters using an RS485 connection and Node.js applications. This module not only collects data from SMA solar inverters but also provides comprehensive functionality to log problems and adjust inverter settings through a JavaScript interface. Data from SMA solar inverters is read using a Linux device such as a Raspberry Pi, or an X86 intel powered computer, and made available to your Node.js applications.

## How does it work

This application is written as a native Node.js addon using C++ bindings to the SMA provided YASDI libraries to query and control SMA inverters connected via RS485. The module provides a complete JavaScript API to interact with the inverters, including data collection, settings adjustment, and problem logging. Data can be processed in real-time within your Node.js application or written to log files on disk as configured by your application.

## Features

- Read all available data points from SMA inverters
- Adjust inverter settings through JavaScript
- Log problems and errors
- Support for multiple connected inverters
- Configurable polling intervals
- Comprehensive error handling

## Usage

1. convert serial port using RS485 to usb adapter ( guide below )
2. install yasdi library ( instructions below )
3. install required packages `npm install`
4. build wrapper `npm run build`
5. use `SMInverter` module in your code ( example in `examples/server.js` or Usage section below )

## Prerequisites

### Requirements

- Node.js env
- RS485 to USB adapter ( instructions below )
- YASDI library to be installed on your system ( instructions below )
- Valid YASDI configuration file ( instructions below )

### RS485 to USB converter

The SMA (as most inverters) can use RS485 as a means to communicate data and settings. RS485 is a signalling protocol that allows many devices to share the same physical pair of wires, in a master/slave relationship. See -> http://www.usb-serial-adapter.org/ for further information.

When an RS485 to USB converter has been plugged in, on Linux systems the device will connect to something like `/dev/ttyUSB0`. To check:

```
sudo tail -f /var/log/syslog
```

...then plug in the converter

You should see a line like: `usb 1-1.4: ch341-uart converter now attached to ttyUSB0`. This means that the RS485 serial port can be accessed by the logical device `/dev/ttyUSB0`. Alternatively, try: `dmesg | grep tty`

### Inverter to RS485 (DB9) Physical Connection

Reference for the wiring:

https://support.tigoenergy.com/hc/en-us/articles/200337847-Monitor-SMA-Inverter-via-RS485

If your computer has an RS485 port, then the inverter can be connected directly to this port. The inverter is connected using 3 wires to the RS485 DB9 port on the computer. DO NOT connect the inverter RS485 to a RS232 port. They are not voltage compatible and damage will probably occur. For this to happen, you need a device like these:

- http://www.robotshop.com/en/db9-female-breakout-board.html
- https://core-electronics.com.au/db9-female-breakout-board.html
- https://www.amazon.com/Female-Breakout-Board-Screw-Terminals/dp/B00CMC1XFU

Each and every RS485 port that uses DB9 has a different pinout. So you have to read the actual manual for the physical RS485 port you are using. For example; if using the Advantech UNO 2362G, the following pins are used: Pin1 = D- , Pin 2 = D+ and Pin 5 = GND. All other pins are not connected, so do not connect any other pins. So to wire it all up:

1. Make sure the Advantech is turned off
2. D+ Pin from the SMA Inverter (should be Pin 2 on the RS485 inverter interface) to Pin 2 of the RS485 DB-9 (Female) on the Advantech UNO 2362G
3. GND Pin from the SMA Inverter (should be Pin 5 on the RS485 inverter interface) to Pin 5 of the RS485 DB-9 (Female) on the Advantech UNO 2362G
4. D- Pin from the SMA Inverter (should be Pin 7 on the RS485 inverter interface) to Pin 1 of the RS485 DB-9 (Female) on the Advantech UNO 2362G

Confirm the physical serial port by running the command `dmesg | grep tty`. As stated previously, it should return something like `/dev/ttyS1` if using a serial com port, or something like `/dev/ttyUSB0` if using a 485/USB serial converter.

## Building and Installing YASDI Library

The YASDI software (See: http://www.sma.de/en/products/monitoring-control/yasdi.html) allows communications with the SMA inverter via RS485. do the following to install the libraries and utilities. You may need to install all the build tools as follows:

```bash
sudo apt-get update
sudo apt-get install -y build-essential git cmake
```

Then install the YASDI software:

```bash
cd yasdi
unzip yasdi-1.8.1build9-src.zip
# Edit the file yasdi/include/packet.h and change Line 38:
# from struct TDevice * Device ...to.... struct _TDevice * Device
cd projects/generic-cmake
mkdir build-gcc
cd build-gcc
cmake ..
make
sudo make install
sudo ldconfig
```

Create a configuration file:

```bash
cd
vi yasdi.in
```

With the following content:

```
[DriverModules]
Driver0=yasdi_drv_serial

[COM1]
Device=/dev/ttyUSB0
Media=RS485
Baudrate=1200
Protocol=SMANet
```

Test the installation:

```bash
sudo yasdishell yasdi.in
```

In the 'yasdi.in' file above, change the 'Device=...' to whatever the device name is, as per the instructions above.

## Usage

A ready example express server supporting all interactions is in `examples/server.js`

```javascript
const SMInverter = require("./index.js"); // adjust to fit path to source

async function example() {
  // Create a new instance with config file path
  const inverter = new SMInverter({
    configPath: "/path/to/yasdi.ini",
    debugLevel: 1, // Optional debug level
  });

  // Initialize the connection
  await inverter.initialize();

  // Detect inverters (specify number of devices to look for)
  await inverter.detectDevices(1);

  // Get list of devices
  const devices = await inverter.getDevices();
  console.log("Detected devices:", devices);

  if (devices.length > 0) {
    // Get live data from the first device
    const data = await inverter.getDeviceData(devices[0].handle);
    console.log("Inverter data:", data);

    // Get channel information
    const channelInfo = await inverter.getChannelInfo(
      devices[0].handle,
      "grid power"
    );
    console.log("Channel info:", channelInfo);

    // Example of setting a value (if channel is writable)
    const result = await inverter.setChannelValue(
      devices[0].handle,
      "writable_channel_name",
      value
    );
    console.log("Set result:", result);
  }

  // Always shut down when finished
  await inverter.shutdown();
}

example().catch(console.error);
```

## API Documentation

### Constructor

```javascript
const inverter = new SMInverter(options);
```

Options:

- `configPath`: Path to the YASDI configuration file (default: '/home/user/yasdi.ini')
- `debugLevel`: Debug level, 0-3 (default: 0)

### Methods

- `initialize()`: Initialize the YASDI SDK
- `detectDevices(deviceCount)`: Detect devices, with optional device count (default: 1)
- `getDevices()`: Get all detected devices
- `getDeviceData(deviceHandle)`: Get live data from a device (pass handle or name)
- `getChannelInfo(deviceHandle, channelName)`: Get info about a specific channel
- `setChannelValue(deviceHandle, channelName, value)`: Set a value for a channel
- `shutdown()`: Shut down the SDK
