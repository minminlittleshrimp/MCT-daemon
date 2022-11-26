# MCT Daemon

This is the tool for log catching and writing between MCT subcriber and MCT publisher.
MCT subcriber acts as a log consumer, and it consumes logs generated by MCT publisher.
## Overview

- A **MCT Publisher** makes use of MCT Library by utilizing the API provided and
generates logs as a log generator.
- The **MCT Library** provides API for MCT Publisher
- The **MCT Daemon** is the MCT interface for communication between Publisher
and Subcriber. It collects logs generated by Publisher and buffers them upon
Subcriber's request. MCT Daemon also accepts control messages from Subcriber to
adjust Daemon configuration or configure Publisher's behaviors.
- A **MCT Subcriber** fetches logs from MCT Daemon and consumes logs. It could send
some control messages for configuration or trigger features (log catcher).

## Get Started

Follow this link to know how to [install and build](#install-and-build)
MCT Daemon and [run a MCT demo](#mct-demo) if you like, whatever :v

### Install and build

You need to install cmake for the build

```bash
sudo apt-get install cmake
```

Then proceed to download/clone MCT if you haven't already.

```bash
cd /path_to_ws
git clone https://github.com/minminlittleshrimp/MCT-daemon.git
```

To build and install the MCT daemon, follow these steps:

```bash
$cd /path_to_ws/mct-daemon
$mkdir build
$cd build
$cmake ..
$make
optional:
$sudo make install
$sudo ldconfig
```

### MCT demo
You could find a demo in [how to set up a MCT demo
setup](doc/mct_demo_setup.md).

### Configure, Control and Interface

| Document | Description |
|----|----|
| *Configuration* ||
|[mct-daemon](doc/mct-daemon.md) | MCT-Daemon and how to run |
|[mct.conf](doc/mct.conf.md) | Reflect user case by config MCT framework|
| *Control running instances of MCT*||
|[mct-publisher](doc/mct-receive.1.md)| Receive and store MCT logs |
|[mct-subcriber](doc/mct-control.1.md)| Send MCT logs to the Daemon |
|[mct-logkeeper-ctrl](doc/mct-logstorage-ctrl.1.md)| Send a control signal to mount/unmount a device |

## Known issues

In case issues found, please contact via email or directly on the repo.

## Software/Hardware

Developed and tested with Ubuntu Linux 16 64-bit / Intel PC.

## Contact

Luu Quang, Minh <minhmark47@gmail.com>,

Hoang Quang, Chanh <>,

Nguyen Nhu, Thuan <>