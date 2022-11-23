# MCT Build Options

Change a value with: cmake -D\<Variable\>=\<Value\>, E.g.

```bash
cmake .. -DWITH_<OPTION>=ON
-DCMAKE_INSTALL_PREFIX=/usr
```

## General Options

Option | Value | Comment
:--- | :--- | :---
BUILD\_SHARED\_LIBS | ON | Set to OFF to build static libraries
MCT\_PUBLISHER                         | mct         | Set publisher for process not run as root
CMAKE\_INSTALL\_PREFIX            | /usr/local

## Command Line Tool Options

 Option | Value | Comment
 :--- | :--- | :---
WITH\_MCT\_LOGKEEPER\_CTRL\_UDEV | OFF            | PROTOTYPE! Set to ON to build



