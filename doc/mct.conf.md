% MCT.CONF(5)

# NAME

**mct.conf** - MCT daemon configuration file

# DESCRIPTION

The MCT daemon is the central application which gathers logs and traces from different applications, stores them temporarily or permanently and transfers them to a MCT client application, which could run directly on the GENIVI system or more likely on some external tester device.

The configuration file mct.conf allows to configure the different
runtime behaviour of the mct-daemon. It is loaded during startup of mct-daemon.

# GENERAL OPTIONS

## Verbose

Start daemon in debug mode, so that all internal debug information is printed out on the console.

    Default: Off

## Daemonize

If set to 1 MCT daemon is started in background as daemon. This option is only needed in System V init systems. In systemd based startup systems the daemon is started by spawning own process.

    Default: 0

## SendSerialHeader

If set to 1 MCT daemon sends each MCT message to the client with prepanding the serial header "DLS0x01".

    Default: 0

## SendContextRegistration

If set to 1 each context which is registered from an application in the MCT daemon generates a message to inform the MCT client about the new context.

    Default: 1

## SendMessageTime

If set to 1 DLt daemon sends each second a MCT control message to the client with the current timestamp from the system.

    Default: 0

## ECUId

This value sets the ECU Id, which is sent with each MCT message.

    Default: ECU1

## SharedMemorySize

This value sets the size of the shared memory, which is used to exchange MCT messages between applications and daemon. This value is defined in bytes. If this value is changed the system must be rebooted to take effect.

    Default: 100000

## PersistanceStoragePath

This is the directory path, where the MCT daemon stores its runtime configuration. Runtime configuration includes stored log levels, trace status and changed logging mode.

    Default: /tmp

## LoggingMode

The logging console for internal logging of mct-daemon. 0 = log to stdout, 1 = log to syslog, 2 = log to file (see LoggingFilename), 3 = log to stderr

    Default: 0

## LoggingLevel

The internal log level, up to which logs are written. LOG_EMERG = 0, LOG_ALERT = 1, LOG_CRIT = 2, LOG_ERR = 3, LOG_WARNING = 4, LOG_NOTICE = 5, LOG_INFO = 6, LOG_DEBUG = 7

    Default: 6

## LoggingFilename

If LoggingMode is set to 2 logs are written to the file path given here.

    Default: /tmp/mct.log

## TimeOutOnSend

Socket timeout in seconds for sending to clients.

    Default: 4

## RingbufferMinSize

The minimum size of the Ringbuffer, used for storing temporary MCT messages, until client is connected.

    Default: 500000

## RingbufferMaxSize

The max size of the Ringbuffer, used for storing temporary MCT messages, until client is connected.

    Default: 10000000

## RingbufferStepSize

The step size the Ringbuffer is increased, used for storing temporary MCT messages, until client is connected.

    Default: 500000

## Daemon FIFOSize

The size of Daemon FIFO (MinSize: depend on pagesize of system, MaxSize: please check `/proc/sys/fs/pipe-max-size`)
This is only supported for Linux.

    Default: 65536

## ContextLogLevel

Initial log-level that is sent when an application registers. MCT_LOG_OFF = 0, MCT_LOG_FATAL = 1, MCT_LOG_ERROR = 2, MCT_LOG_WARN = 3, MCT_LOG_INFO = 4, MCT_LOG_DEBUG = 5, MCT_LOG_VERBOSE = 6

    Default: 4

## ContextTraceStatus

Initial trace-status that is sent when an application registers. MCT_TRACE_STATUS_OFF = 0, MCT_TRACE_STATUS_ON = 1

    Default: 0

## ForceContextLogLevelAndTraceStatus

Force log level and trace status of contexts to not exceed "ContextLogLevel" and "ContextTraceStatus". If set to 1 (ON) whenever a context registers or changes the log-level it has to be lower or equal to ContextLogLevel.

    Default: 0

## InjectionMode

If set to 0, the injection mode (see [here](./mct_for_developers.md#MCT-Injection-Messages)) is disabled.

    Default: 1

# GATEWAY CONFIGURATION

## GatewayMode

Enable Gateway mode

    Default: 0

## GatewayConfigFile

Read gateway configuration from another location

    Default: /etc/mct_gateway.conf

# Permission configuration

MCT daemon runs with e.g.
 User: genivi_mct
 Group: genivi_mct

MCT user applications run with different user and group than mct-daemon but with supplimentory group: mct_user_apps_group

<basedir>/mct FIFO will be created by mct-daemon with
    User: genivi_mct
    Group: mct_user_apps_group
    Permission: 620

so that only mct-daemon can read and only processes in mct_user_apps_group can write.

<basedir>/mctpipes will be created by mct-daemon with
    User: genivi_mct
    Group: genivi_mct
    Permission: 3733 (i.e Sticky bit and SGID turned on)

<basedir>/mctpipes/mct<PID> FIFO will be created by mct application (user lib) with
    User: <user of the application>
    Group: genivi_mct (inherited from <basedir>mctpipes/ due to SGID)
    Permission: 620

Thus MCT user applications (and also or attackers) can create the mct<PID> FIFO
(for communication from mct-daemon to MCT user application) under <basedir>/mctpipes/. Since sticky bit is set the applications who creates the FIFO can only rename/delete it.

Since SGID of <basedir>/mctpipes is set the group of mct<PID> FIFO will be genivi_mct which enables mct daemon to have write permission on all the mct<PID> FIFO.

One mct user application cannot access mct<PID> FIFO created by other mct user application(if they run with different user).

Owner group of daemon FIFO directory(Default: /tmp/mct) (If not set, primary group of mct-daemon process is used).
Application should have write permission to this group for tracing into mct. For this opton to work, mct-daemon should have this group in it's supplementary group.

## DaemonFifoGroup

Owner group of daemon FIFO directory
(If not set, primary group of mct-daemon process is used)
Application should have write permission to this group for tracing into mct
For this opton to work, mct-daemon should have this group in it's Supplementary group

    Default: group of mct-daemon process (/tmp/mct)

# CONTROL APPLICATION OPTIONS

## ControlSocketPath

Path to control socket.

    Default: /tmp/mct-ctrl.sock

# OFFLINE TRACE OPTIONS

## OfflineTraceDirectory

Store MCT messages to local directory, if not set offline Trace is off.

    Default: /tmp

## OfflineTraceFileSize

This value defines the max size of a offline trace file, if offline trace is enabled. This value is defined in bytes. If the files size of the current used log file is exceeded, a new log file is created.

    Default: 1000000

## OfflineTraceMaxSize

This value defines the max offline Trace memory size, if offline trace is enabled. This value is defined in bytes. If the overall offline trace size is excedded, the oldest log files are deleted, until a new trace file fits the overall offline trace max size.

    Default: 4000000

## OfflineTraceFileNameTimestampBased

Filename timestamp based or index based. 1 = timestamp based, 0 = index based

    Default: Function is disabled

# LOCAL CONSOLE OUTPUT OPTIONS

## PrintASCII

Prints each received MCT message from the application in ASCII to the local console. This option should only be anabled for debugging purpose.

    Default: Function is disabled

## PrintHex

Prints each received MCT message from the application in ASCII to the local console. The payload is printed in Hex. This option should only be anabled for debugging purpose.

    Default: Function is disabled

## PrintHeadersOnly

Prints each received MCT message from the application in ASCII to the local console. Only the header is printed. This option should only be anabled for debugging purpose.

    Default: Function is disabled

# SERIAL CLIENT OPTIONS

## RS232DeviceName

If this value is set to a serial device name, e.g. /dev/ttyS0, a serial port is used for logging to a client.

    Default: Serial port for logging is disabled

## RS232Baudrate

The used serial baud rate, if serial loggin is enabled. The RS232DeviceName must be set to enable serial logging.

    Default: 115200

## RS232SyncSerialHeader

If serial logging is enabled, each received MCT message is checked to contain a serial header. If the MCT message contains no serial header, the message is ignored.

    Default: Function is disabled

# TCP CLIENT OPTIONS

## TCPSyncSerialHeader

Each received MCT message on a TCP connection is checked to contain a serial header. If the MCT message contains no serial header, the message is ignored.

    Default: Function is disabled

# ECU SOFTWARE VERSION OPTIONS

## SendECUSoftwareVersion

Periodically send ECU version info. 0 = disabled, 1 = enabled

    Default: Function is disabled

# PathToECUSoftwareVersion

Absolute path to file storing version information - if disabled the MCT version will be send.

    Default: Function is disabled.

# TIMEZONE INFO OPTIONS

# SendTimezone

Periodically send timezone info. 0 = disabled, 1 = enabled

    Default: Function is disabled

# OFFLINE LOGSTORAGE OPTIONS

## OfflineLogstorageMaxDevices

Maximum devices to be used as offline logstorage devices. 0 = disabled, 1 .. MCT_OFFLINE_LOGSTORAGE_MAX_DEVICES

    Default: 0 (Function is disabled)

## OfflineLogstorageDirPath

Path to store MCT offline log storage messages.

    Default: off

## OfflineLogstorageTimestamp

Appends timestamp in log file name. 0 = disabled, 1 = enabled

    Default: 0

## OfflineLogstorageDelimiter

Appends delimiter in log file name, only punctuation characters allowed.

    Default: _

## OfflineLogstorageMaxCounter

Wrap around value for log file count in file name.

    Default: UINT_MAX

## OfflineLogstorageCacheSize

Maximal used memory for log storage cache in KB.

    Default: 30000 KB

## UDPConnectionSetup

Enable or disable UDP connection. 0 = disabled, 1 = enabled

## UDPMulticastIPAddress

The address on which daemon multicasts the log messages

## UDPMulticastIPPort

The Multicase IP port. Default: 3491

# AUTHOR

Alexander Wenzel (alexander.aw.wenzel (at) bmw (dot) de)

# COPYRIGHT

Copyright (C) 2015 BMW AG. License MPL-2.0: Mozilla Public License version 2.0 <http://mozilla.org/MPL/2.0/>.

# BUGS

See Github issue: <https://github.com/GENIVI/mct-daemon/issues>

# SEE ALSO

**mct-daemon(1)**, **mct-system(1)**
