# NAME

**mct-log-reader** - Console based message reader for MCT Logging

# SYNOPSIS

**mct-log-reader** \[**-h**\] \[**-a**\] \[**-x**\] \[**-m**\] \[**-s**\] \[**-o** filename\] \[**-c** limit\] \[**-v**\] \[**-y**\] \[**-b** baudrate\] \[**-e** ecuid\] \[**-p** port\] hostname/serial_device_name

# DESCRIPTION

Receive MCT messages from MCT daemon and print or store the messages.

## OPTIONS

-h

: Display a short help text.

-a

: Print MCT file; payload as ASCII.

-x

:   Print MCT file; payload as hex.

-m

:   Print MCT file; payload as hex and ASCII.

-s

:   Print MCT file; only headers.

-o

:   Output messages in new MCT file.

-c

:   Set limit when storing messages in file. When limit is reached, a new file is opened. Use K,M,G as suffix to specify kilo-, mega-, giga-bytes respectively, e.g. 1M for one megabyte (Default: unlimited).

-v

:   Verbose mode.

-S

:   Send message with serial header (Default: Without serial header)

-R

:   Enable resync serial header

-y

:   Serial device mode.

-b

:   Serial device baudrate (Default: 115200).

-e

:   Set central ID (Default: RECV).

-p

:   Port TCP communication (Default: 3490).
# EXAMPLES

Print message headers received from a mct-daemon running on localhost::
    **mct-log-reader -s localhost**

Print message headers received from a serial interface::
    **mct-log-reader -s -y /dev/ttySO**

Store incoming messages in file(s) and restrict file sizes to 1 megabyte. If limit is reached, log.mct will be renamed into log.0.mct, log.1.mct, ... No files will be overwritten in this mode::
    **mct-log-reader -o log.mct -c 1M localhost**

## Space separated filter file
Testing and implementing.

## Json filter file
Only available, when builded with cmake option `WITH_EXTENDED_FILTERING`.

Testing and implementing.
# EXIT STATUS

Non zero is returned in case of failure.

# NOTES

Be aware that mct-log-reader will never delete any files. Instead, it creates a new file.

# AUTHOR

Luu Quang Minh (leader)

Hoang Quang Chanh

Nguyen Nhu Thuan

# COPYRIGHT


# BUGS

See Github issue: <>

**mct-daemon**
