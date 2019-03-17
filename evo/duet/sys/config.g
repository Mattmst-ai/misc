; Configuration file for Duet WiFi (firmware version 1.21)
; executed by the firmware on start-up
;
; generated by RepRapFirmware Configuration Tool v2 on Mon Mar 11 2019 21:01:27 GMT-0700 (Pacific Daylight Time)

; General preferences
G90                                          ; Send absolute coordinates...
M83                                          ; ...but relative extruder moves

M667 S1                                      ; Select CoreXY mode

; Network
M550 P"CoreXY"                               ; Set machine name
M551 P"reprap"                               ; Set password
M552 S1                                      ; Enable network
;*** Access point is configured manually via M587
M586 P0 S1                                   ; Enable HTTP
M586 P1 S1                                   ; Enable FTP
M586 P2 S1                                   ; Enable Telnet

; Drives
M569 P0 S1                                   ; Drive 0 goes forwards
M569 P1 S1                                   ; Drive 1 goes forwards
M569 P2 S1                                   ; Drive 2 goes forwards
M569 P3 S1                                   ; Drive 3 goes forwards
M350 X16 Y16 Z16 E16 I1                      ; Configure microstepping with interpolation
M92 X80.00 Y80.00 Z4000.00 E436.54           ; Set steps per mm
M566 X900.00 Y900.00 Z12.00 E120.00          ; Set maximum instantaneous speed changes (mm/min)
M203 X6000.00 Y6000.00 Z180.00 E1200.00      ; Set maximum speeds (mm/min)
M201 X500.00 Y500.00 Z20.00 E250.00          ; Set accelerations (mm/s^2)
M906 X1000.00 Y1000.00 Z1000.00 E1000.00 I30 ; Set motor currents (mA) and motor idle factor in per cent
M84 S30                                      ; Set idle timeout

; Axis Limits
M208 X20 Y20 Z0 S1                           ; Set axis minima
M208 X320 Y320 Z250 S0                       ; Set axis maxima

; Endstops
M574 X2 Y2 S1                                ; Set active high endstops

; Z-Probe
M574 Z1 S2                                   ; Set endstops controlled by probe
M307 H3 A-1 C-1 D-1                          ; Disable heater on PWM channel for BLTouch
M558 P9 H5 F120 T6000                        ; Set Z probe type to bltouch and the dive height + speeds
G31 P500 X20 Y5 Z2.5                         ; Set Z probe trigger value, offset and trigger height
M557 X35:215 Y35:195 S20                     ; Define mesh grid

; Heaters
M305 P0 T100000 B4138 R4700                  ; Set thermistor + ADC parameters for heater 0
M143 H0 S120                                 ; Set temperature limit for heater 0 to 120C
M305 P1 T100000 B4138 R4700                  ; Set thermistor + ADC parameters for heater 1
M143 H1 S280                                 ; Set temperature limit for heater 1 to 280C

; Fans
M106 P0 S0.3 I0 F500 H-1                     ; Set fan 0 value, PWM signal inversion and frequency. Thermostatic control is turned off
M106 P1 S1 I0 F500 H1 T45                    ; Set fan 1 value, PWM signal inversion and frequency. Thermostatic control is turned on

; Tools
M563 P0 D0 H1                                ; Define tool 0
G10 P0 X0 Y0 Z0                              ; Set tool 0 axis offsets
G10 P0 R0 S0                                 ; Set initial tool 0 active and standby temperatures to 0C

; Automatic saving after power loss is not enabled

; Custom settings are not configured

