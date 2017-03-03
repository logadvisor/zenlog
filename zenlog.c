#include <math.h>

#include "libnet/include/libnet.h"

#define ZENLOG_PATTERN_RANDOM     0
#define ZENLOG_PATTERN_COSINE     1
#define ZENLOG_PATTERN_CONSTANT   2
#define ZENLOG_PATTERN_FOLLOW     3
#define ZENLOG_PATTERN_FAST       4

#define MAX_MSG_LENGTH          8192
#define MINUTES_PER_DAY         1440

struct _options {
    libnet_t *l;
    struct libnet_stats ls;
    char errbuf[LIBNET_ERRBUF_SIZE];

    u_long src_ip;
    u_long dst_ip;
    u_short src_port;
    u_short dst_port;
    u_short src_ip_count;

    u_short min_rate;
    u_short max_rate;

    u_long msg_count;
    u_short loop_count;           // 0 = continuous, otherwise go "loop" times

    u_long usleep;
    u_short pattern;
    u_short verbose;

    FILE *logfile;
} options;

void usage(char *name) {
    fprintf(stderr, "\nUsage: %s\t-s src[+count][:port]\n\t\t-d dst[:port]\n\t\t-m min_mps\n\t\t-M max_mps\n\t\t-c number_of_messages_to_send\n\t\t-S microseconds_to_sleep_between_send\n\t\t-l number_of_times_to_loop_the_file\n\t\t-p random|cosine|constant|follow|fast\n\t\t-f logfile\n\t\t-v\n", name);
}

int parse_args(int argc, char **argv) {
    extern char *optarg; 
    char *cp;
    int i;

    // sets everything to 0
    // side effect: loop = 0, continuous
    // side effect: pattern = 0, RANDOM
    if (memset(&options, 0, sizeof(struct _options)) != &options) {
        fprintf(stderr, "Error: Unable to zero out options array");
        return (EXIT_FAILURE);
    }

    while ((i = getopt(argc, argv, "s:d:m:M:c:S:l:p:f:v")) != EOF) {
        switch (i) {
        case 'v':
            options.verbose = 1;
            break;

        case 's':
            // we expect the source ip syntax to be
            // ip[+count][:port], where
            // ip       = ip of the source address, if omitted, use host's ip
            // +count   = create "count" ips, so if ip is 10.1.1.1 and count is
            //            3, then we create 4 source ips, 10.1.1.1,2,3,4
            //            if omitted, just the ip itself
            // :port    = source port, if omitted, use 514/udp, always udp

            // if : exists, then we assume the src_port follows
            cp = strrchr(optarg, ':');
            if (cp) {
                *cp++ = 0;
                options.src_port = (u_short) atoi(cp);
            }

            // if + exists, then we assume ip count follows
            cp = strrchr(optarg, '+');
            if (cp) {
                *cp++ = 0;
                options.src_ip_count = (u_short) atoi(cp);
            }

            if ((options.src_ip = libnet_name2addr4(options.l, optarg, LIBNET_RESOLVE)) == -1) {
                fprintf(stderr, "Error: Bad source IP address: %s\n", optarg);
                return (EXIT_FAILURE);
            }
            break;

        case 'd':
            // if : exists, then we assume the src_port follows
            cp = strrchr(optarg, ':');
            if (cp) {
                *cp++ = 0;
                options.dst_port = (u_short) atoi(cp);
            }

            if ((options.dst_ip = libnet_name2addr4(options.l, optarg, LIBNET_RESOLVE)) == -1) {
                fprintf(stderr, "Error: Bad destination IP address: %s\n", optarg);
                return (EXIT_FAILURE);
            }
            break;

        case 'm':
            options.min_rate = (u_short) atoi(optarg);
            break;

        case 'M':
            options.max_rate = (u_short) atoi(optarg);
            break;

        case 'c':
            options.msg_count = (u_long) atol(optarg);
            break;

        case 'l':
            options.loop_count = (u_short) atoi(optarg);
            break;

        case 'S':
            options.usleep = (u_long) atol(optarg);
            break;

        case 'p':
            if (strncmp(optarg, "random", 6) == 0) {
                options.pattern = ZENLOG_PATTERN_RANDOM;
            } else if (strncmp(optarg, "cosine", 6) == 0) {
                options.pattern = ZENLOG_PATTERN_COSINE;
            } else if (strncmp(optarg, "constant", 8) == 0) {
                options.pattern = ZENLOG_PATTERN_CONSTANT;
            } else if (strncmp(optarg, "follow", 6) == 0) {
                options.pattern = ZENLOG_PATTERN_FOLLOW;
            } else if (strncmp(optarg, "fast", 4) == 0) {
                options.pattern = ZENLOG_PATTERN_FAST;
            } else {
                fprintf(stderr, "Error: Bad pattern: %s\n", optarg);
                return (EXIT_FAILURE);
            }
            break;

        case 'f':
            options.logfile = fopen(optarg, "r");
            if (options.logfile == NULL) {
                fprintf(stderr, "Error: Bad filename: %s\n", optarg);
                return (EXIT_FAILURE);
            }
            break;

        default:
            fprintf(stderr, "Error parsing command line arguments\n");
            usage(argv[0]);
            return -1;
        }
    }

    if (options.logfile == NULL) {
        fprintf(stderr, "Error: No filename found in argument\n");
        return (EXIT_FAILURE);
    }

    if (options.min_rate == 0) {
        fprintf(stderr, "Error: No min_mps found in argument\n");
        return (EXIT_FAILURE);
    }

    if (options.max_rate == 0) {
        fprintf(stderr, "Error: No max_mps found in argument\n");
        return (EXIT_FAILURE);
    }

    if (options.src_ip == 0) {
        if((options.src_ip = libnet_get_ipaddr4(options.l)) == -1) {
            fprintf(stderr, "Error: Unable to get source IP address: %s\n", libnet_geterror(options.l));
            return (EXIT_FAILURE);
        }
    }

    if (options.dst_ip == 0) {
        if ((options.dst_ip = libnet_name2addr4(options.l, "127.0.0.1", LIBNET_DONT_RESOLVE)) == -1) {
            fprintf(stderr, "Error: Unable to get destination IP address: 127.0.0.1\n");
            return (EXIT_FAILURE);
        }
    }

    if (options.src_port == 0) {
        options.src_port = 514;
    }

    if (options.dst_port == 0) {
        options.dst_port = 514;
    }

    if (options.src_ip_count == 0) {
        options.src_ip_count = 1;
    }

    return 0;
} 

int get_messages_per_second() {
    if (options.pattern == ZENLOG_PATTERN_RANDOM) {
        int diff = options.max_rate - options.min_rate;
        if (diff != 0) {
            u_long r = libnet_get_prand(LIBNET_PRu32) % diff;
            return r+options.min_rate;
        } else {
            return options.min_rate;
        }
    } else if (options.pattern == ZENLOG_PATTERN_CONSTANT) {
        return ((options.max_rate + options.min_rate) / 2);
    } else if (options.pattern == ZENLOG_PATTERN_COSINE) {
        struct timeval tv;
        struct timezone tz;
        long cur_minute = 0;

        gettimeofday(&tv, &tz);

        if (((tv.tv_sec / 60) - tz.tz_minuteswest) > cur_minute) {
            cur_minute = ((tv.tv_sec / 60) - tz.tz_minuteswest);

            return ((
                     (1.0 - cos(
                                ((double) cur_minute / MINUTES_PER_DAY)
                                * 2
                                * M_PI
                               )
                     ) 
                     / 2
                     * (options.max_rate - options.min_rate)
                    ) 
                    + options.min_rate
                   );
        } else {
            return options.min_rate;
        }
    } else if (options.pattern == ZENLOG_PATTERN_FAST) {
        return 1000000;
    }
}

void send_message(char *msg) {
    u_long src_ip;
    int i;
    libnet_ptag_t udp = 0, ip = 0, ipo = 0;
    u_int msg_len = strlen(msg);

    libnet_clear_packet(options.l);

    udp = libnet_build_udp(options.src_port,
                           options.dst_port,
                           LIBNET_UDP_H + msg_len,
                           0,
                           (u_char *)msg,
                           msg_len,
                           options.l,
                           udp);

    if (udp == -1) {
        fprintf(stderr, "Error: Can't build UDP header: %s\n", libnet_geterror(options.l));
        return;
    }

    for (i = 0; i < options.src_ip_count; i++) {
        src_ip = options.src_ip + i;

        ip = libnet_build_ipv4(LIBNET_IPV4_H + msg_len + LIBNET_UDP_H,
                               0,
                               242,
                               0,
                               64,
                               IPPROTO_UDP,
                               0,
                               src_ip,
                               options.dst_ip,
                               NULL,
                               0,
                               options.l,
                               ip);
        if (ip == -1) {
            fprintf(stderr, "Can't build IP header: %s\n", libnet_geterror(options.l));
            continue;
        }

        /*
         *  Write it to the wire.
         */
        if (options.verbose) fprintf(stderr, "%d byte packet, ready to go\n",
            libnet_getpacket_size(options.l));

        int c = libnet_write(options.l);

        if (c == -1) {
            fprintf(stderr, "Write error: %s\n", libnet_geterror(options.l));
        } else {
            if (options.verbose) fprintf(stderr, "Wrote %d byte UDP packet; check the wire.\n", c);
        }
    }
}

void zen_logs() {
    int loop_count = 0;
    int msg_count = 0;
    char msg[MAX_MSG_LENGTH];
    int i;
    
    if (options.pattern == ZENLOG_PATTERN_RANDOM) {
        libnet_seed_prand(options.l);
    }

    while (1) {
        if (options.msg_count != 0 && options.msg_count <= msg_count) {
            return;
        }

        int send = get_messages_per_second();

        if ((options.msg_count != 0) && (options.msg_count < msg_count+send)) {
            send = options.msg_count - msg_count;
        }

        if (options.verbose) fprintf(stderr, "send = %d\n", send);

        u_long sleep = 1000000 / send;

        for (i = 0; i < send; i++) {
            if (fgets(msg, MAX_MSG_LENGTH, options.logfile) != NULL) {
                send_message(msg);
            } else {
                if (feof(options.logfile)) {
                    rewind(options.logfile);
                    i--;

                    loop_count++;
                    if (options.loop_count != 0 && options.loop_count <= loop_count) {
                        return;
                    }
                }
            }

            usleep(sleep);
            msg_count++;
        }

        if (options.usleep > 0) {
            usleep(options.usleep);
        }
    }
}

int main(int argc, char *argv[]) {
    int ret = parse_args(argc, argv);
    if (ret == EXIT_FAILURE) {
        usage(argv[0]);
        exit(ret);
    }

    // root required
    options.l = libnet_init(LIBNET_RAW4, NULL, options.errbuf);

    if (options.l == NULL) {
        fprintf(stderr, "Error: libnet_init() failed: %s\n", options.errbuf);
        exit(EXIT_FAILURE);
    }

    zen_logs();

    libnet_destroy(options.l);
    return (EXIT_SUCCESS);
}

