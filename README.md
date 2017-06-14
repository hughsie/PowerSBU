# PowerSBU

These tools allows you to visualize the usage of solar, battery and utility
power over different time spans, allowing you to effectively manage
battery banks and solar panel arrays.

With a pluggable design, PowerSBU can support different types of hardware.

# Connecting to a remote server

You can connect to a machine and redirect the D-Bus methods using gabriel:

    gabriel -h server -d unix:path=/var/run/dbus/system_bus_socket
    DBUS_SYSTEM_BUS_ADDRESS="unix:abstract=/tmp/gabriel" ./src/sbu-gui
