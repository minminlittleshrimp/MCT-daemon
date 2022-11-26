
# NAME

**mct-log-converter** - Convert MCT Logging files into ASCII

# SYNOPSIS

**mct-log-converter** \[**-h**\] \[**-a**\] \[**-x**\] \[**-m**\] \[**-s**\] \[**-t**\] \[**-o** filename\] \[**-v**\] \[**-c**\] \[**-f** filterfile\] \[**-b** number\] \[**-e** number\] \[**-w**\] file1 \[file2\] \[file3\]

# DESCRIPTION

Read MCT files, print MCT messages as ASCII and store the messages again.
Use Ranges and Output file to cut MCT files.
Use two files and Output file to join MCT files.

# OPTIONS

-h

:   Display a short help text.

-a

:   Print MCT file; payload as ASCII.

-x

:   Print MCT file; payload as hex.

-m

:   Print MCT file; payload as hex and ASCII.

-s
    Print MCT file; only headers.

-o

:    Output messages in new MCT file.

-v

:    Verbose mode.

-c

:    Count number of messages.

-f

:   Enable filtering of messages.

-b

:   First messages to be handled.

-e

:   Last message to be handled.

-w

:   Follow mct file while file is increasing.

-t

:   Handling the compressed input files (tar.gz).

# EXAMPLES

Convert MCT file into ASCII:
    **mct-log-converter -a mylog.mct**

Cut a specific range, e.g. from message 1 to message 3 from a file called log.mct and store the result to a file called newlog.mct:
    **mct-log-converter -b 1 -e 3 -o newlog.mct log.mct**

Paste two mct files log1.mct and log2.mct to a new file called newlog.mct:
    **mct-log-converter -o newlog.mct log1.mct log2.mct**

Handle the compressed input files and join inputs into a new file called newlog.mct:
    **mct-log-converter -t -o newlog.mct log1.mct compressed_log2.tar.gz**


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

See Github issue: <>

# SEE ALSO

**mct-daemon**