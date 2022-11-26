# MCT Build Options

## General Options

Option | Value | Comment
:--- | :--- | :---
BUILD\_SHARED\_LIBS | OFF | Set to ON to build dynamic libraries
 WITH\_MCT\_CONSOLE | ON  | Set to ON to build src/console binaries
WITH\_MCT\_CONSOLE\_SBTM | OFF | Set to ON to build mct-sortbytimestamp under src/console
MCT\_USER | mct-vjpro | Set user for process not run as root
CMAKE\_INSTALL\_PREFIX | /usr/local
CMAKE\_BUILD\_TYPE | RelWithDebInfo
CMAKE\_HOST\_SYSTEM\_PROCESSOR | x86_64
CMAKE\_SYSTEM\_PROCESSOR | x86_64
WITH\_MCT\_LOGSTORAGE\_CTRL\_UDEV | OFF | PROTOTYPE! Set to ON to build logstorage control application with udev support
WITH\_MCT\_LOGSTORAGE\_CTRL\_PROP | OFF | PROTOTYPE! Set to ON to build logstorage control application with proprietary support
MCT\_IPC | FIFO | Set to UNIX_SOCKET for unix_socket IPC (Default: FIFO, path: /tmp)
WITH\_MCT\_DISABLE\_MACRO | OFF | Set to ON to build code without Macro interface support