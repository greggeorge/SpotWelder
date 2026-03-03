# SpotWelderControl v1 #

Using the *Beetle* (Arduino Leonardo compatible) to provide fixed duration or adhoc pulse to Solid State Relay (SSR) to control the duration of the spot weld current.

The SSR used is a  FOTEK SSR-40 Solid State Relay.  

Using the 'Weld' button pulse can be generated for a fixed duration (default 1000ms - 1 sec) or using the 'Untimed Weld' button
a weld of between 20ms and 15 seconds (15000 ms) can be applied.  We using the Untimed Weld button, the actual time spend welding is displayed after the weld so the user can alter
the default weld time if required using this time as a guide.  There is an 'Up' and 'Down' button for changing the weld duration when using the 'Weld' button.


The transformer used is a 600VA rewound Microwave oven transformer.  The output winding is 1.5 turns and of about 15mm diameter multi strand copper cable giving an open circuit voltage of 1.3VAC

A circuit schematic has been provided in KiCad format.

Information is displayed on a 16x2 character LCD I2C display
