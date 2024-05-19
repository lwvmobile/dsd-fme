

## Audio Plumbing

While using DSD-FME to demodulate and decode RF Audio, you will find it necessary to properly plumb (or route) audio from each input source and output sink properly. This mini tutorial aims to help you do such when using Pulse Audio.

You will first want to locate and run the virtualsink.sh script file via `sh virtualsink.sh` command. This will create two null-sink, or 'virtual sinks' for us to plumb audio from point A to point B. 

Here are screenshots of a proper setup using pavucontrol, or "Pulse Audio Volume Control", having voice input from the encoder listening to your microphone, the RF Output going into the "Virtual_Sink" null-sink, and on the playback side, playing back voice to our speakers, with RF modulated audio being monitored on "Virtual-Sink" null-sink.

![Audio 1](https://github.com/lwvmobile/dsd-fme/blob/aw_dev/examples/pavucontrol_plumbing1.png)

![Audio 2](https://github.com/lwvmobile/dsd-fme/blob/aw_dev/examples/pavucontrol_plumbing2.png)

The general idea is that we want to route RF modulated audio into our Virtual_Sink and have our decoder listen to the same sink. Voice Output should always be to Audio Hardware (speakers, headphones, etc).

Note, that using the virtualsink.sh will create these sinks for us for the duration of your computers boot. If you reboot, they will not persist. Configuration files can be modified to always have these sinks available, or the `virtualsink.sh` script can be executed on login, but you must do so at your own risk and own research, it is out of the scope of this tutorial to create persistent sinks.

In case of the usage of a headless environment and pavucontrol is not viable, you may use alternatives such as pulsemixer which is similar in functionality, just as a CLI tool and not a GUI tool.

Users can now use the `-O` CLI argument to list out all pulse audio input sources and output sinks and pass an optional argument alongside `pulse`, appending the name or index number to select the device each time. This method will work in both Linux environments as well as Cygwin builds (making sure to start the pulseaudio.exe server in the back first with the specified .bat file or command, see below).

For Example:

The Output (truncated) of `dsd-fme -O`

```
=======[ Output Device #1 ]=======
Description: Family 17h (Models 00h-0fh) HD Audio Controller Analog Stereo
Name: alsa_output.pci-0000_0d_00.3.analog-stereo
Index: 1

=======[ Output Device #4 ]=======
Description: Virtual_Sink1
Name: virtual_sink1
Index: 4

=======[ Output Device #5 ]=======
Description: Virtual_Sink2
Name: virtual_sink2
Index: 5

=======[ Input Device #2 ]=======
Description: Family 17h (Models 00h-0fh) HD Audio Controller Analog Stereo
Name: alsa_input.pci-0000_0d_00.3.analog-stereo
Index: 2

=======[ Input Device #5 ]=======
Description: Monitor of Virtual_Sink1
Name: virtual_sink1.monitor
Index: 5

=======[ Input Device #6 ]=======
Description: Monitor of Virtual_Sink2
Name: virtual_sink2.monitor
Index: 6
```

You might use any of the following for input and output options:

```
-i pulse
-i pulse:6
-i pulse:virtual_sink2.monitor

-o pulse
-o pulse:1
-o pulse:alsa_output.pci-0000_0d_00.3.analog-stereo
```

Something to keep in mind is that often times, you only need to set these values once, as a pulse audio server (or plugin) can often times remember these settings, even across reboots, as long as the same hardware is present for future use (real or null-sink).

<!-- TODO: Add This later -->
<!-- Cygwin Note: Running the supplied `0p - start-pulse-audio-backend.bat` file will not only start up the pulse audio server for you, but also setup your null-sinks, and enumerate all devices for you to use, making it an easy selection for your start up .bat files. See any notes in the precomplied .zip file for more information. -->