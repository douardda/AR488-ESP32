
AR488-ESP32 GPIB Controller
===========================

The AR488-ESP32 GPIB controller is an ESP32/Arduino-based controller for
interfacing with `IEEE488 GPIB <https://en.wikipedia.org/wiki/IEEE-488>`_
devices, implementing the Prologix_ serial communication protocol.

The code supports ESP32_ boards and has been tested on several `ESP32 and ESP32S2
devkits`_. These boards come with support for wifi connection and serial over Bluetooth
(not supported on ESP32S2.)

Being based on the orignal `AR488`_ code (by Twilight-Logic and others), it
also supports AVR based arduino boards and has been tested on Arduino_ Uno_,
Nano_, `Mega 2560`_, `Micro 32U4`_ boards, but these boards having very
constrained resources cann support all the features the ESP32 based ones, so it
is strongly recommemded to

To build an interface, at least one of the aforementioned Arduino or ESP32
board will be required to act as the interface hardware. Connecting to an
instrument will require a 16 core cable and a suitable `IEEE488 connector
<https://en.wikipedia.org/wiki/IEEE-488#Connectors>`_. This can be salvaged
from an old GPIB cable or purchased from electronics parts suppliers.
Alternatively, a PCB board can be designed to incorporate a directly mounted
IEEE488 connector.

The interface firmware can optionally support the
`SN75160 <https://www.ti.com/product/SN75160B>`_ and
`SN75161 <https://www.ti.com/product/SN75161B>`_ or
`SN75162 <https://www.ti.com/product/SN75162B>`_ GPIB transceiver integrated
circuits.

Details of construction and the mapping of board pins to GPIB control signals
and the data bus are explained in the :ref:`Building an AR488-ESP32 GPIB Interface`
section.

The interface firmware supports standard Prologix_ commands and adheres closely
to the Prologix syntax but there are some minor differences and a number of
additopnal commands. Details of all commands can be found in the :ref:`Command
Reference` section.

.. _Arduino: https://arduino.cc
.. _Uno: https://store.arduino.cc/products/arduino-uno-rev3
.. _Nano: https://store.arduino.cc/products/arduino-nano
.. _`Mega 2560`: https://store.arduino.cc/products/arduino-mega-2560-rev3
.. _`Micro 32U4`: https://store.arduino.cc/products/arduino-micro

.. _ESP32: https://www.espressif.com
.. _`ESP32 and ESP32S2 devkits`: https://www.espressif.com/en/products/devkits
.. _`AR488`: https://github.com/Twilight-Logic/AR488
.. _Prologix: http://prologix.biz/gpib-usb-4.2-faq.html


Driver Installation
-------------------

Most recent OS (Windows, Linux and MacOS) will recognize the serial chipsets used by the
boards listed above, so no special driver installation is required.


Firmware Upgrades
-----------------

The firmware is upgradeable via the Arduino IDE or the platformio_ `command
line tool`_ in the usual manner, however an AVR programmer can also be used to
upload the firmware to the Arduino microcontroller. Pre-compiled firmwares for
some boards are available from https://github.com/douardda/AR488-ESP32/releases
but you will probably want to customize and compile your own versino of the
firmware suitable for your setup and needs.

.. _platformio: https://platformio.org
.. _`command line tool`: https://docs.platformio.org/en/latest/core/index.html


Client Software
---------------

The interface can be accessed via a number of software client programs:

- Terminal software (e.g. PuTTY)
- `EZGPIB <http://www.ulrich-bangert.de/html/downloads.html>`_
- `KE5FX GPIB Configurator <http://www.ke5fx.com/>`_
- `Luke Mester’s HP3478 Control <https://mesterhome.com/gpibsw/hp3478a/>`_
- Python scripts
- Anything else that can use the Prologix syntax!

When using direct USB-based serial connection, terminal clients can connect via
a virtual serial/COM port and should be set to 115200 baud, no parity, 8 data
bits and 1 stop bit when connecting to the interface. On Linux, the port will
be a TTY device such as ``/dev/ttyUSB0`` or ``/dev/ttyACM0``.

For using Bluetooth or Wifi to connect to the AR488-ESP32, please refer to the
:ref:`Remote Connection` section.

Specific considerations apply when using an Arduino based interface with EZGPIB and the
KE5FX toolkit. These are described in the :ref:`EZGPIB` section.


Operating Modes
---------------

The interface can operate in both controller and device modes.

Controller mode
+++++++++++++++

In this mode the interface can control and read data from various instruments including
Digital multimeters (DMMs), oscilloscopes, signal generators and spectrum analyzers.
When powered on, the controller sends out an IFC (Interface Clear) to the GPIB bus to
indicate that it is now the Controller-in-Charge (CIC).

All AR488/Prologix specific commands are preceded with the ``++`` sequence and
terminated with a carriage return (CR), linefeed (LF) or both (CRLF). Commands are sent
to or affect the currently addressed instrument which can be specified with the
``++addr`` command (see command :ref:`++help` for more information).


By default, the controller is at GPIB address 0.

As with the Prologix interface, the controller has an auto mode that allows data to be
read from the instrument without having to repeatedly issue ``++read`` commands. After
``++auto 1`` is issued, the controller will continue to perform reading of measurements
automatically after the next ``++read`` command is used and using the parameters that
were specified when issuing that command.

Device mode
+++++++++++

The interface supports device mode allowing it to be used to send data to GPIB devices
such as plotters via a serial USB connection. All device mode commands are supported.


Transmission of data
--------------------

Interrupting transmission of data
+++++++++++++++++++++++++++++++++

While reading of data for the GPIB bus is in progress, the interface will still respond
to the ``++`` sequence that indicates a command. For example, under certain conditions
when the instrument is addressed to talk (e.g. when ``eos`` is set to 3 [no terminator
character] and the expected termination character is not received from the instrument,
or read with eoi and the instrument is not configured to assert ``eoi``, or ``auto
mode`` is enabled), data transmission may continue indefinitely. The interface will
still respond to the ``++`` sequence followed by a command (e.g. ``++auto 0`` or
``++rst``). Data transmission can be stopped and the configuration can then be adjusted.


Sending Data and Special characters
+++++++++++++++++++++++++++++++++++

Carriage return (``CR``, ``0x0D``, ``13d``), newline (a.k.a. linefeed) (``LF``,
``0x0A``, ``10d``), escape (``0x1B``, ``27d``) and ``+`` (``0x2B``, ``43d``) are special
control characters.

Carriage return and newline terminate command strings and direct instrument commands,
whereas a sequence of two ``+`` precedes a command token. Special care needs to be taken
when sending binary data to an instrument, because in this case we do not want control
characters to prompt some kind of action. Rather, they need to be treated as ordinary
and added to the data that is to be transmitted.

When sending binary data, the above mentioned characters must be escaped by preceding
them with a single escape (``0x1B``, ``27d``) byte. For example, consider sending the
following binary data sequence::

  54 45 1B 53 2B 0D 54 46

It would be necessary to escape the 3 control characters and send the following::

  54 45 **1B** 1B 53 **1B** 2B **1B** 0D 54 46

Without these additional escape character bytes, the special control characters present
in the sequence will be interpreted as actions and an incomplete or incorrect data
sequence will be sent.

It is also necessary to prevent the interface from terminating the binary data sequence
with a carriage return and newline (``0D 0A``) as this will confuse most instruments.
The command ``++eos 3`` can be used to turn off termination characters. The command
``++eos 0`` will restore default operation. See the command help that follows for more
details.


Receiving data
++++++++++++++

Binary data received from an instrument is transmitted over GPIB and then via serial
over USB to the host computer PC unmodified. Since binary data from instruments is not
usually terminated by CR or LF characters (as is usual with ASCII data), the EOI signal
can be used to indicate the end of the data transmission. Detection of the EOI signal
while reading data can be accomplished with the ``++read eoi`` command, while an
optional character can be added as a delimiter with the ``++eot_enable`` command (see
the command help that follows). The instrument must be configured to send the EOI
signal. For further information on enabling the sending of EOI see your instrument
manual.

Listen-only and talk-only (lon and ton) modes
+++++++++++++++++++++++++++++++++++++++++++++

In device mode, the interface supports both “listen-only” and “talk-only” modes (for
more details see the ``++lon`` and ``++ton`` commands. These modes are not addressed
modes and do not require a GPIB address to be set. Therefore if any GPIB address is
already set, it is simply ignored. Moreover, when in either of these modes, devices are
not controlled by the CIC. Data characters are sent using standard GPIB handshaking ,
but GPIB commands are ignored. The bus acts as a simple one to many transmission medium.
In lon mode, the device will receive any data placed on the bus by any talker, including
any other addressed device or controller. Since only ONE talker can exist on the bus at
a time, there can only be one device in "talk-only" mode on the bus, however multiple
"listen-only" devices can be present and all will receive the data sent by the talker.

Wireless communication
----------------------

The AR488-ESP32 interface can communicate via Bluetooth or Wifi (beware not all
ESP32 boards have support for Bluetooth). Details on remote communication can
be found in the :ref:`Remote Connection` section.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   configuration
   quickstart
   commands
   macros
   build
   bluetooth
   tools
   appendixes

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
