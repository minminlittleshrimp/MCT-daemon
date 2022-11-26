# NAME

**mct-daemon** - Minh Chanh Thuan daemon (3 idiots)

# SYNOPSIS

**mct-daemon** \[**-h**\] \[**-d**\] \[**-c** filename\] \[**-p** port\]

# DESCRIPTION

This MCT Daemon will act as a central tracer by storing logs from log-writer and
later buffer these logs in the need of the log-reader.

## OPTIONS

-h

:   Display a short help text.

-d

:   Daemonize mode ON.

-c

:   Load an alternative configuration file. Default: /etc/mct.conf.

-p

:   Port to monitor for incoming requests (Default: 3490)

# EXAMPLES

Start MCT daemon in background mode:
    **mct-daemon -d**

Start MCT daemon with own configuration:
    **mct-daemon -c ~/<my_conf_file>.conf**

Start MCT daemon listening on custom port 3500:
    **mct-daemon -p 3500**

# EXIT STATUS

Non zero is returned in case of failure.

# AUTHOR

Luu Quang Minh (leader)

Hoang Quang Chanh

Nguyen Nhu Thuan

# COPYRIGHT


# BUGS

See Github issue: <>

# SEE ALSO

**mct.conf**
