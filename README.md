# Exporter485

A Prometheus exporter that uses RS-485 to query modbus devices and export arbitrary metrics, created out of my need to
run on an old Raspberry PI connected to my solar charge controllers.

## Getting started

The following instructions are for Debian/Ubuntu machines. Install build dependencies:

    apt-get install \
        cmake \
        libyaml-dev libcyaml-dev \
        libevent-dev \
        libmodbus-dev

Build:

    mkdir build
    cd build
    cmake ..
    make

Run in dry-mode, without connecting to an actual serial device and using sample configuration for EPEver/EPSolar PV
charge controller:

    ./exporter485 --dry-run --config-file ../examples/epever.yaml
    curl -v 'http://localhost:9485/metrics?module=epever_controller&target=1'

Or, run on a real system with a specific serial device:

    ./exporter485 --config-file ../examples/epever.yaml --device /dev/ttyXRUSB1

## Configuration

This works as a [multi target exporter](https://prometheus.io/docs/guides/multi-target-exporter/), with the target
parameter specifying the target device's modbus ID.
The config file defines modules, which map modbus registers to Prometheus metrics.
Additional configuration can be provided as command line arguments. To get more help, run:

    ./exporter485 --help

## TODO

This is a really early work in progress, but it works for me. Other things I considered adding are:

[ ] Better logging.
[ ] Batched modbus register reading.
[ ] Better error handling.
[ ] A status page.
[ ] Exporter introspection metrics.
