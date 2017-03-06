# zenlog
log replay utility to replay old log data back via UDP.  Pick source and destination IPs and Ports, Message rates (minimum and maximum) per second, and number of messages to send.  This is typically used for testing log collection software.

Usage: ./zenlog	-s src[+count][:port]
		-d dst[:port]
		-m min_mps
		-M max_mps
		-c number_of_messages_to_send
		-S microseconds_to_sleep_between_send
		-l number_of_times_to_loop_the_file
		-p random|cosine|constant|follow|fast
		-f logfile

Examples:

$ zenlog -s 192.168.0.1 -d 127.0.0.1:514 -f example.log -m 5 -M 50
