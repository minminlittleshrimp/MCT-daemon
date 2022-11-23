# NAME

**mct-daemon** - Minh Chanh Thuan daemon (3 idiots)

# SYNOPSIS

**mct-daemon** \[**-h**\] \[**-d**\] \[**-c** filename\] \[**-p** port\]

# DESCRIPTION

The MCT Daemon acts as a central data transferring space where logs from Publisher will be kept temporarily or permanently based on requests from Subcriber.

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

Luu Quang Minh, Hoang Quang Chanh, Nguyen Nhu Thuan

# COPYRIGHT


# BUGS

See Github issue: <>

# SEE ALSO

**mct.conf**
