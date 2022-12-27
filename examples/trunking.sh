#example trunking sh scripts (SDR++ and RTL Input) -T = trunking
#make sure to put a # in front of all lines except the one you wish to use

#P25 P1 FDMA CC with C4FM 
#dsd-fme -i tcp -U 4532 -T -N 2> log.ans
#dsd-fme -i rtl:0:850M:26:-2:8:0:6020 -T -N 2> log.ans

#P25 P1 FDMA CC with QPSK (Simulcast) 
#dsd-fme -i tcp:192.168.7.8:7355 -U 4532 -T -mq -N 2> log.ans
#dsd-fme -i rtl:0:855.625M:26:-2:12:0:6020 -T -mq -N 2> log.ans

#P25 P2 TDMA CC with QPSK (Simulcast)
#dsd-fme -i tcp:192.168.7.8:7355 -U 4532 -T -f2 -m2 -N 2> log.ans
#dsd-fme -i rtl:0:855.625M:26:-2:12:0:6020 -T -f2 -m2 -N 2> log.ans

#DMR TIII, Con+, Cap+
dsd-fme -fs -i tcp -U 4532 -T -C dmr_t3_chan.csv -G group.csv -N 2> log.ans
#dsd-fme -fs -i rtl:0:450M:26:-2:8:0:6020 -T -C connect_plus_chan.csv -G group.csv -N 2> log.ans

#EDACS/EDACS-EA Digital Only
#dsd-fme -fp -i tcp:192.168.7.5:7355 -U 4532 -T -C edacs_channel_map.csv -G group.csv -N 2> log.ans
#dsd-fme -fp -i rtl:0:850M:26:-2:24:0:6020 -T -C edacs_channel_map.csv -G group.csv -N 2> log.ans

#NXDN48 Trunking with Standard Band Plan (Channels 0-800)
#dsd-fme -fi -i tcp -T -U 4532 -N 2> log.ans
#dsd-fme -fi -i rtl:0:450M:26:-2:8:75:6020 -T -G group.csv -N 2> log.ans

#NXDN48 Trunking with Channel Map (Custom Channels > 800)
#dsd-fme -fi -i tcp -T -U 4532 -C channel_map.csv -N 2> log.ans
#dsd-fme -fi -i rtl:0:450M:26:-2:8:75:6020 -T -G group.csv -C channel_map.csv -N 2> log.ans

#Notes: parameters after -i tcp can be omitted if using localhost as the default hostname to connect
#and 7355 as the default port number. -U is the port number for RIGCTL and will mirror the tcp hostname
#running under the assumption that the user will be connnecting the VFO to the same instance of SDR++
#You can also specify an ip address to connect to a remote SDR++ instance, make sure that the settings
#inside of SDR++ match, i.e., change in SDR++ from 'localhost' to an IP to bind to (machine's IP address)

#Note2: RTL input should only be used on strong signal, the SDR++ VFO will perform much better on marginal signal

#Having a -G group.csv is purely optional, but useful if you want to block groups, or give groups names
