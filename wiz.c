#include <argp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "wiz.h"

const char *argp_program_version = "wiz_cli v0.0.1";
const char *argp_program_bug_address = "<info@finfaq.net>";
const char doc[] = "wiz is a cli tool for controlling wiz lights.";

static struct argp_option options[] = {
    {"color", 'c', "COLOR", 0, "Color name (r, g, b, red, green, or blue) or RGB (0-255,0-255,0-255) color value", 0},
    {"discover", 'd', "TIMEOUT,MAX_DEVS", 0, "Broadcast a discovery signal to the network and print responses to stdout until TIMEOUT (in seconds) elapses or MAX_DEVS responses have been received", 0},
    {"broadcast", 'b', 0, 0, "Broadcasts the command to all devices on the current network, regardless of whether they appear in the config file.", 0},
    {"ips", 'i', "ADDRESS", 0, "Comma-separated list of device IP addresses", 0},
    {"kelvin", 'k', "KELVIN", 0, "Temperature in kelvins, must be in [2000, 9000)", 0},
    {"list", 'l', 0, 0, "Lists the devices to which the command is sent", 0},
    {"name", 'n', "[NAME...]", 0, "Device name or comma-separated list of names; if not specified, signals are sent to all devices named in the config file", 0},
    {"off", 'q', 0, 0, "Send a turn-off signal", 0},
    {"on", 'o', 0, 0, "Send a turn-on signal", 0},
    {"scene", 's', "SCENE", 0, "Name of the scene", 0},
    {"room", 'r', "[ROOM...]", 0, "Name of the room or comma-separated list of rooms", 0},
    {0}, // "This should be terminated by an entry with zero in all fields."
};
static error_t parse_opt(int, char *, struct argp_state *);
static struct argp argp = {options, parse_opt, 0, doc, 0, 0, 0};
static int max(int a, int b) { return (a > b) ? a : b; }
static int clamp(int min, int max, int n)
{
    n = (n > min) ? n : min;
    return (n < max) ? n : max;
}

int main(int argc, char *argv[])
{
    int exit_status = EXIT_SUCCESS;
    // parse arguments
    struct arg_vals args = {};
    error_t err = argp_parse(&argp, argc, argv, 0, 0, &args);
    if (err)
    {
        perror(NULL);
        return EXIT_FAILURE;
    }

    // check if the program is operating in discovery mode, broadcast mode, or ip mode.
    // these modes do not read the device config file.

    if (args.discover) { return broadcast_udp_wait(INFO, sizeof(INFO), args.seconds, args.num_devs); }

    if (args.broadcast) { return msg_all(args); }

    if (args.ips != NULL) { return use_ips(args); }

    
    // load the device configs into memory
    struct stat fstat;
    if ((stat(WIZ_PATH, &fstat)) < 0)
    {  
        fprintf(stderr, "unable to stat device configuration file\n");
        perror(NULL);
        return EXIT_FAILURE;
    }
    if (fstat.st_size == 0) {
        fprintf(stderr, "device configuration file is empty\n");
        return EXIT_FAILURE;
    }
    char *buf = malloc(fstat.st_size + 1);

    FILE *fp = fopen(WIZ_PATH, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "unable to open device configuration file\n");
        perror(NULL);
        return EXIT_FAILURE;
    }
    if (fread(buf, fstat.st_size, 1, fp) == 0)
    {
        fprintf(stderr, "unable to read device configuration file\n");
        return EXIT_FAILURE;
    }
    if (fclose(fp) < 0)
    {
        fprintf(stderr, "unable to close device configuration file\n");
        perror(NULL);
        return EXIT_FAILURE;
    }

    // parse devices names/ips from the config file
    device devs[MAX_DEVS];
    int n = 0;
    n = parse_csv(buf, fstat.st_size + 1, devs, args.name, args.room);
    if (n < 0)
    {
        perror(NULL);
        exit_status = EXIT_FAILURE;
        goto end;
    }
    if (n == 0 ) {
        fprintf(stderr, "no devices read from the configuration file\n");
        exit_status = EXIT_FAILURE;
        goto end;
    }

    // update devs to reflect args
    update_dev(devs, args);
    char msg[MAX_REQ];
    int mlen = req_msg(msg, devs[0]);
    
    

    if (args.list)
    {
        printf("Devices\n");
        printf("NAME\tIP ADDRESS\tROOM\n");
        for (int i = 0; i < n; i++)
        {
            printf("%s\t%s", devs[i].name, devs[i].ip);
            if (devs[i].room == NULL)
                printf("\n");
            else
                printf("\t%s\n", devs[i].room);
        }
    }

    if (send_cmds(msg, mlen, devs, n) < 0)
    {
        fprintf(stderr, "error sending cmds\n");
        exit_status = EXIT_FAILURE;
    }

end:
    free(buf);

    return exit_status;
}

int send_cmds(char *msg, int mlen, device devs[], int num_devs)
{
    int res = 0;
    // CREATE NEW UDP SOCKET
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        return sockfd;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);

    for (int i = 0; i < num_devs; i++)
    {
        if (inet_aton(devs[i].ip, &(sin.sin_addr)) == 0)
        {
            fprintf(stderr, "error parsing ip address\n");
            res = -1;
            break;
        }
        socklen_t len = sizeof(sin);
        if (sendto(sockfd, (void *)msg, mlen, 0, (struct sockaddr *)(&sin), len) < 0)
        {
            fprintf(stderr, "error sending request\n");
            perror(NULL);
            res = -1;
            break;
        };
    }

    if (close(sockfd) < 0)
    {
        res = -1;
    }

    return res;
}

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    struct arg_vals *arg_info = state->input;

    switch (key)
    {
    case 'b':
        arg_info->broadcast = true;
        break;
    case 'c':
        if (init_color(&arg_info->col, arg))
        {
            fprintf(stderr, "unable to parse color\n");
            argp_usage(state);
        }
        arg_info->change_col = true;
        break;
    case 'd':
        arg_info->discover = true;
        sscanf(arg, "%d,%d", &(arg_info->seconds), &(arg_info->num_devs));
        break;
    case 'i':
        arg_info->ips = arg;
        break;
    case 'k':
        arg_info->kelvin = clamp(2000, 8999, atoi(arg));
        break;
    case 'l':
        arg_info->list = true;
        break;
    case 'n':
        arg_info->name = arg;
        break;
    case 'o':
        arg_info->turn_on = true;
        break;
    case 'q':
        arg_info->turn_off = true;
        break;
    case 'r':
        arg_info->room = arg;
        break;
    case 's':
        arg_info->scene = str_scene(arg);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

int init_color(color *col, char *s)
{
    // first check if s matches a named color.
    int r = 0, g = 0, b = 0;
    if (strcmp(s, "r") == 0)
        r = 255;
    else if (strcmp(s, "g") == 0)
        g = 255;
    else if (strcmp(s, "b") == 0)
        b = 255;
    else if (strcmp(s, "red") == 0)
        r = 255;
    else if (strcmp(s, "green") == 0)
        g = 255;
    else if (strcmp(s, "blue") == 0)
        b = 255;

    if (r || g || b)
    {
        col->r = r;
        col->g = g;
        col->b = b;
        return 0;
    }

    // then check if s can be parsed as r,g,b
    int n = sscanf(s, "%hhu,%hhu,%hhu", &col->r, &col->g, &col->b);
    return (n != 3);
};

device init_dev( struct arg_vals args) {
    device dev = {};
    if (args.change_col)
    {
        
        dev.op = CMD_COLOR;
        dev.col = args.col;
        
    }
    else if (args.turn_off)
    {
        dev.op = CMD_OFF;
    }
    else if (args.kelvin > 1999)
    {
        dev.op = CMD_KELVIN;
        dev.kelvin = args.kelvin;
    }
    else if (args.turn_on)
    {
        dev.op = CMD_ON;
    }
    else if (args.scene > BAD_SCENE)
    {
        dev.op = CMD_SCENE;
        dev.scene = args.scene;
    }
    else
    {
        dev.op = CMD_ON;
    }
    return dev;
}

void update_dev(device *dev, struct arg_vals args) {
if (args.change_col)
    {
        
        dev->op = CMD_COLOR;
        dev->col = args.col;
        
    }
    else if (args.turn_off)
    {
        dev->op = CMD_OFF;
    }
    else if (args.kelvin > 1999)
    {
        dev->op = CMD_KELVIN;
        dev->kelvin = args.kelvin;
    }
    else if (args.turn_on)
    {
        dev->op = CMD_ON;
    }
    else if (args.scene > BAD_SCENE)
    {
        dev->op = CMD_SCENE;
        dev->scene = args.scene;
    }
    else
    {
        dev->op = CMD_ON;
    }
}


int req_msg(char buf[], device dev)
{
    int n;
    switch (dev.op)
    {
    case CMD_OFF:
        strcpy(buf, OFF);
        return sizeof(OFF);
    case CMD_ON:
        strcpy(buf, ON);
        return sizeof(ON);
    case CMD_COLOR:
        n = sprintf(buf, COLOR, dev.col.r, dev.col.g, dev.col.b);
        return (n < 1) ? -1 : n + 1;
    case CMD_KELVIN:
        n = sprintf(buf, KELVIN, dev.kelvin);
        return (n < 1) ? -1 : n + 1;
    case CMD_SCENE:
        n = sprintf(buf, SCENE, dev.scene);
        return (n < 1) ? -1 : n + 1;
    }
    return -1;
}

int parse_csv(char *data, int n, device devs[], char *search_names, char *search_rooms)
{
    int d = 0;
    char *name;
    char *ip;
    char *room;
    bool find_name = true;
    int i = 0;

find_name:
    name = &data[i];
    for (; i < n; i++)
    {
        if (data[i] == ',')
        {
            data[i] = '\0';
            i++;
            goto find_ip;
        }
    }

find_ip:
    ip = &data[i];
    for (; i < n; i++)
    {
        if (data[i] == ',')
        {
            data[i] = '\0';
            i++;
            goto find_group;
        }
        else if (data[i] == '\n')
        {
            data[i] = '\0';
            if (search_rooms != NULL)
            {
                i++;
                goto find_name;
            }
            else if (search_names != NULL)
            {
                if (is_in(name, search_names))
                {
                    devs[d].name = name;
                    devs[d].ip = ip;
                    devs[d].room = NULL;
                    d++;
                }
            }
            else
            {
                devs[d].name = name;
                devs[d].ip = ip;
                devs[d].room = NULL;
                d++;
            }
            i++;
            goto find_name;
        }
    }

find_group:
    room = &data[i];
    for (; i < n; i++)
    {
        if (data[i] == '\n')
        {
            data[i] = '\0';
            if (search_names != NULL && search_rooms != NULL)
            {
                if (is_in(name, search_names) && is_in(room, search_rooms))
                {
                    devs[d].name = name;
                    devs[d].ip = ip;
                    devs[d].room = room;
                    d++;
                }
            }
            else if (search_names != NULL)
            {
                if (is_in(name, search_names))
                {
                    devs[d].name = name;
                    devs[d].ip = ip;
                    devs[d].room = room;
                    d++;
                }
            }
            else if (search_rooms != NULL)
            {
                if (is_in(room, search_rooms))
                {
                    devs[d].name = name;
                    devs[d].ip = ip;
                    devs[d].room = room;
                    d++;
                }
            }
            else
            {
                devs[d].name = name;
                devs[d].ip = ip;
                devs[d].room = room;
                d++;
            }
            i++;
            goto find_name;
        }
    }

    return d;
}

int parse_ips(char *src, device devs[]) {
    int n = 0;
    int i = 0;
    int src_len = strlen(src);
    device new = {};
    for (; i < src_len && n < MAX_DEVS; n++) {
        devs[n] = new;
        devs[n].ip = &src[i];
        while (i < src_len) {
            if (src[i] == ',') {
                src[i] = '\0';
                i++;
                break;
            }
            i++;
        }
    }
    return n;
}

static char *scene_strs[] = {
    "ocean",
    "romance",
    "sunset",
    "party",
    "fireplace",
    "cozy",
    "forest",
    "pastel_colors",
    "wake_up",
    "bedtime",
    "warm_white",
    "daylight",
    "cool_white",
    "night_light",
    "focus",
    "relax",
    "true_colors",
    "tv_time",
    "plant_growth",
    "spring",
    "summer",
    "fall",
    "deep_dive",
    "jungle",
    "mojito",
    "club",
    "christmas",
    "halloween",
    "candle_light",
    "golden_white",
    "pulse",
    "steampunk",
    "diwali",
};

scene str_scene(char *s)
{
    for (char *p = s; *p; p++)
        *p = tolower(*p);

    for (int i = 0; i < MAX_SCENE; i++)
    {
        if (strcmp(s, scene_strs[i]) == 0)
            return i + 1;
    }
    return BAD_SCENE;
}

bool is_in(char *s, char *list)
{
    int i, j;
    i = 0, j = 0;

    char c;
    while (list[j])
    {
        while (s[i] && (s[i] == list[j]))
        {
            i++;
            j++;
        }

        if (!s[i] && !list[j])
        {
            return true;
        }
        if (!list[j])
        {
            return false;
        }
        if (!s[i] && list[j] == ',')
        {
            return true;
        }

        while (c = list[++j])
        {
            if (c == ',')
            {
                i = 0;
                j++;
                break;
            }
        }
    }

    return false;
}

int broadcast_udp_wait(char *msg, int mlen, int timeout, int max_resps)
{
    int res = 0;
    // CREATE NEW UDP SOCKET
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror(NULL);
        return -1;
    }

    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0)
    {
        perror(NULL);
        close(sockfd);
        return -1;
    }

    if (timeout <= 0)
        timeout = 1;

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror(NULL);
        fprintf(stderr, "error setting socket\n");
        close(sockfd);
        return -1;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = INADDR_BROADCAST;
    
    if (sendto(sockfd, (void *)msg, mlen, 0, (struct sockaddr *)(&sin), sizeof(sin)) < 0)
    {
        perror(NULL);
        fprintf(stderr, "send error\n");
        close(sockfd);
        return -1;
    }

    char buf[1024 + 1] = "";
    if (max_resps <= 0)
        max_resps = MAX_DEVS;
    for (int i = 0; i < max_resps; i++)
    {
        int fromlen = sizeof(sin);
        ssize_t n = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr *)(&sin), &fromlen);
        if (n < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                break;
            }
            fprintf(stderr, "recv error\n");
            close(sockfd);
            return -1;
        }
        char x[20] = "";
        printf("%s\n", inet_ntop(AF_INET, &sin.sin_addr.s_addr, x, INET_ADDRSTRLEN));
    }
    close(sockfd);
    return res;
}


int msg_all(struct arg_vals a) {
    device dev = init_dev(a);
    char buf[MAX_REQ];
    int n = req_msg(buf, dev);
    return  broadcast_udp(buf, n);
}


int broadcast_udp(char *msg, int mlen)
{
    int res = 0;
    // CREATE NEW UDP SOCKET
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror(NULL);
        return -1;
    }

    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0)
    {
        perror(NULL);
        close(sockfd);
        return -1;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = INADDR_BROADCAST;
    
    if (sendto(sockfd, (void *)msg, mlen, 0, (struct sockaddr *)(&sin), sizeof(sin)) < 0)
    {
        perror(NULL);
        fprintf(stderr, "send error\n");
        return -1;
    }
    close(sockfd);
    return res;
}

int use_ips(struct arg_vals args) {
    device devs[MAX_DEVS];
    int n = parse_ips(args.ips, devs);
    if (n < 1) {
        fprintf(stderr, "unable to parse ip addresses");
        return EXIT_FAILURE;
    }
    update_dev(devs, args);
    char msg[MAX_REQ];
    int mlen = req_msg(msg, devs[0]);
    return send_cmds(msg, mlen, devs, n);
};