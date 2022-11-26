# NAME

**mct-log-writer** - Console based application for generating logs

# SYNOPSIS

**mct-log-writer** \[**-h**\] \[**-g**\] \[**-a**\] \[**-k**\] \[**-d** delay\] \[**-f** filename\] \[**-S** filesize\] \[**-n** count\] \[**-m** mode\] \[**-l** level\] \[**-A** appID\] \[**-C** contextID\] \[**-t** timeout\] \[**-s** size\] message

# DESCRIPTION

Sends the given message as MCT messages to MCT daemon or prints the raw MCT messages into a local file.

## OPTIONS

-h

: Display a short help text.

-g

: Switch to non-verbose mode (Default: verbose mode).

-a

: Enable local printing of MCT messages (Default: disabled).

-k

: Send marker message.

-d

: Milliseconds to wait between sending messages (Default: 500).

-f

: Use local log file instead of sending to daemon.

-S

: Set maximum size of local log file (Default: UINT\_MAX).

-n

: Number of messages to be generated (Default: 10).

-m

: Set log mode 0=off, 1=external, 2=internal, 3=both.

-l

: Set log level, level=-1..6 (Default: 3).

-A

: Set app ID for send message (Default: LOG).

-C

: Set context ID for send message (Default: TEST).

-t

: Set timeout when sending messages at exit, in ms (Default: 10000 = 10sec).

-r

: Send raw data with specified size instead of string.


# EXAMPLES

Send "HelloWorld" with default settings (10 times, every 0.5 seconds) as MCT message to mct-daemon::

    mct-log-writer HelloWorld

Set app ID to `APP1`, context Id to `TEST` and log level to `error` for send message::

    mct-log-writer -l 2 -A APP1 -C TEST HelloWorld

Send 100 MCT messages every second::

    mct-log-writer -n 100 -d 1000 HelloWorld

Send "HelloWorld" can log to local file with maximum size 1000 bytes::

    mct-log-writer -f helloworld.mct -S 1000 HelloWorld

# EXIT STATUS

Non zero is returned in case of failure.

# Notes

The default descriptions for application and context registration are used irrespective of the IDs that could be set. App will always register with "Test Application for Logging" and context with "Test Context for Logging".

# AUTHOR

Luu Quang Minh (leader)

Hoang Quang Chanh

Nguyen Nhu Thuan

# COPYRIGHT


# BUGS

The log-writer still could not receive any informming message telling
about the connection establishment between log-reader and log-writer.

We are looking for solution!

See Github issue: <>
# SEE ALSO

**mct-daemon**
