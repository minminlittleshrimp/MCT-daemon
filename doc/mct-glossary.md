# Glossary

Throughout the documentation specific terms are used. Those are defined below.

Back to [README.md](../README.md)

Term  | Definition
----- | ----
Application ID | Application Identifier is a unique identifier for an application registered at the MCT Daemon. It is defined as 8bit ASCII character. e.g. "APP1". Each Application can have several sub-components each having a unique context ID
Context ID	| This is a user defined ID to group log and trace messages produced by an application. Each Application ID can own several Context IDs and the Context IDs shall be unique within an Application ID. The identification of the source of a log and trace message is done with a pair of Application ID and Context ID. It is composed by four 8 bit ASCII characters.
Control Message	| A control message is send by a connected client application (e.g. Log Viewer) to the MCT Daemon that includes an action request. E.g. change the Log level of a certain application).
MCT Daemon | The MCT Daemon is the central component which receives all logs and traces from the MCT user applications. The MCT Daemon forwards all logs and traces to a connected MCT client (e.g. Log Viewer) or stores them optionally in a file on the target.
MCT Library | Provides applications (esp. MCT users) with an API to produce MCT messages and to handle MCT Control Messages accordingly.
MCT log-writer | A MCT log-writer is a type of application that produces log messages. It typically uses the MCT library to produce the messages and resembles a control unit.
Injection Message | An injection message is a control message for a specific MCT application.
MCT log-reader | A log-reader is an application connected to the MCT Daemon (MCT Client) that receives log messages and stores (e.g. Logstorage) or displays them. A log-reader can run on the same operating system or on a remote host pc.
Log Level | A log level defines a classification for the severity grade of a log message.
Trace Status | The trace status provides information if a trace message should be send. Supported States are ON or OFF
Verbose / Non-Verbose Mode | The MCT supports Verbose and Non-Verbose Mode. In _Verbose_ mode, all logging data including a type description is provided within the payload. Furthermore, Application ID and Context ID are transferred as part of the message header. In _Non-Verbose_ mode, description about the sender (Application ID, Context ID) as well as static strings and data description is not part of the payload. The log message contains a unique message ID instead which allows a mapping between received log message and information stored in the file.