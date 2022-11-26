# MCT Demo Setup
Back to [README.md](../README.md)

This document demonstrates steps to run MCT Daemon. The Daemon will run silently
in the background, waiting for logs from log-writer and buffer the logs to
log-reader. The demo is for normal case of 1 writer and 1 reader. We are still on
experiment for multi-writers/readers, and the feature will be available in the
next stable release 1.0.1

*NOTE: We assume that you installed MCT (i.e. executed the [two optional steps
after build](../README.md#build-and-install)). Otherwise you have to take care
of the executable paths and explicitly state the library path.*

## Run the MCT Daemon
The MCT Daemon could be triggered as below, do not worry of those Warning messages,
we are still working on them :))
```bash
$ mct-daemon
[18903.248291]~MCT~10357~NOTICE   ~Starting MCT Daemon; MCT Package Version: 1.0.0 STABLE, Package Revision: , build on Nov 26 2022 16:37:35

[18903.248339]~MCT~10357~WARNING  ~Entry does not exist in section: Backend
[18903.248343]~MCT~10357~INFO     ~Optional parameter 'Backend' not specified
[18903.248374]~MCT~10357~INFO     ~FIFO size: 65536
[18903.248381]~MCT~10357~INFO     ~Activate connection type: 5
[18903.248389]~MCT~10357~INFO     ~mct_daemon_socket_open: Socket created
[18903.248396]~MCT~10357~INFO     ~mct_daemon_socket_open: Listening on ip 0.0.0.0 and port: 3490
[18903.248400]~MCT~10357~INFO     ~mct_daemon_socket_open: Socket send queue size: 16384
[18903.248405]~MCT~10357~INFO     ~Activate connection type: 1
[18903.248421]~MCT~10357~INFO     ~Activate connection type: 9
[18903.248426]~MCT~10357~INFO     ~Cannot open configuration file: /tmp/mct-runtime.cfg
[18903.248431]~MCT~10357~INFO     ~Ringbuffer configuration: 500000/10000000/500000
[18903.248566]~MCT~10357~NOTICE   ~Failed to open ECU Software version file.
[18903.248576]~MCT~10357~INFO     ~<Timing packet> initialized with 1 timer
[18903.248578]~MCT~10357~CRITICAL ~Unable to get receiver from 6 connection.
[18903.248581]~MCT~10357~INFO     ~Switched to buffer state for socket connections.
[18903.248586]~MCT~10357~WARNING  ~mct_daemon_applications_load: cannot open file /tmp/mct-runtime-application.cfg: No such file or directory

```
MCT Daemon here opens a named pipe and will read/buffer logs from the pipe. The
connection is established on TCP port 3490, log-reader could collect messages through
the connection in real time.

## MCT-log-writer: mini portable log generator
A simulated control unit - a MCT user - will now use the MCT library to create log
messages and send them through the named pipe for the daemon to collect. Open a
second terminal and run
```bash
Dr.Mint@:~
$ mct-log-writer -n 5 -l 3 "Hello This is mtc-vjpro hehe"
Send 0 Hello This is mtc-vjpro hehe
Send 1 Hello This is mtc-vjpro hehe
Send 2 Hello This is mtc-vjpro hehe
Send 3 Hello This is mtc-vjpro hehe
Send 4 Hello This is mtc-vjpro hehe
```
Then log-writer will send 5 log messages with the log level of WARN (```-l 3```)
Here we still have a problem of not receiving any confirmation from the log-writer
that log-reader has been connected, but logs are still received. We are working on that.

## MCT-log-reader: mini portable log consumer
The MCT-Daemon will keep the logs there in its buffer, we design a ring buffer to fit that.
When the logs are fetched by log-reader, logs will be buffered from daemon:
```bash
$ mct-log-reader -a localhost
2022/11/26 19:03:34.640378  241840146 000 ECU1 DA1- DC1- control response N 1 [service(3842), ok, 02 00 00 00 00]
2022/11/26 19:03:34.640409  189032485 000 ECU1 MCTD INTM log info V 1 [Daemon launched. Starting to output traces...]
2022/11/26 19:03:34.640417  201289364 000 ECU1 LOG- TEST log warn V 2 [0 Hello This is mtc-vjpro hehe]
2022/11/26 19:03:34.640421  201294369 001 ECU1 LOG- TEST log warn V 2 [1 Hello This is mtc-vjpro hehe]
2022/11/26 19:03:34.640424  201299376 002 ECU1 LOG- TEST log warn V 2 [2 Hello This is mtc-vjpro hehe]
2022/11/26 19:03:34.640427  201304383 003 ECU1 LOG- TEST log warn V 2 [3 Hello This is mtc-vjpro hehe]
2022/11/26 19:03:34.640430  201309387 004 ECU1 LOG- TEST log warn V 2 [4 Hello This is mtc-vjpro hehe]
2022/11/26 19:03:34.640432  201324410 000 ECU1 DA1- DC1- control response N 1 [service(3841), ok, 4c 4f 47 00 54 45 53 54 72 65 6d 6f]
2022/11/26 19:03:34.640438  241840146 001 ECU1 MCTD INTM log info V 1 [New client connection 7 established, Total Clients : 1]
```
The client connects to the default port 3490 of localhost to collect all
messages and interprets the payload as ASCII text (```-a```). You can see lots
of additional messages. These are control messages to control the flow between
client and daemon. You will learn about them later. For now, you have set up a
basic example have seen MCT in action.

You can now experiment with this setup. What happens if you start the MCT user
first and (while the MCT user is still running) the daemon?

The log-reader connects to the port of 3490 by default and IP localhost (127.0.0.1), then it collects
all logs and interprets the payload as ASCII text (```-a```). Logs will be showed in stdout, or could
be stored in file using other parser (```-o```)