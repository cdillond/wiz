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
    {"broadcast", 'b', 0, 0, "Broadcasts the command to all devices on the current network, regardless of whether they appear in the config file", 0},
    {"color", 'c', "COLOR", 0, "Color name (r, g, b, red, green, or blue) or RGB (0-255,0-255,0-255) color value", 0},
    {"dimming", 'u', "PERCENT", 0, "Dimming/brightness level percentage (0-100, lower is dimmer)", 0},
    {"discover", 'd', "TIMEOUT,MAX_DEVS", 0, "Broadcast a discovery signal to the network and print responses to stdout until TIMEOUT (in seconds) elapses or MAX_DEVS responses have been received", 0},
    {"ips", 'i', "ADDRESS", 0, "Comma-separated list of device IP addresses", 0},
    {"kelvin", 'k', "KELVIN", 0, "Temperature in kelvins, must be in [2000, 9000)", 0},
    {"list", 'l', 0, 0, "Lists the devices to which the command is sent", 0},
    {"name", 'n', "[NAME...]", 0, "Device name or comma-separated list of names; if not specified, signals are sent to all devices named in the config file", 0},
    {"off", 'q', 0, 0, "Send a turn-off signal", 0},
    {"on", 'o', 0, 0, "Send a turn-on signal", 0},
    {"repeat", 't', "NUMBER", 0, "Number of times to repeat the command"},
    {"room", 'r', "[ROOM...]", 0, "Name of the room or comma-separated list of rooms", 0},
    {"scene", 's', "SCENE", 0, "Name of the scene", 0},
    {"speed", 'v', "SPEED", 0, "Scene transition speed (10-200)", 0},
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

    if (args.discover)
    {
        for (int i = 0; i <= args.repeat; i++)
        {
            exit_status = broadcast_udp_wait(INFO, sizeof(INFO), args.seconds, args.num_devs);
        }
        return exit_status;
    }

    if (args.broadcast)
    {
        for (int i = 0; i <= args.repeat; i++)
        {
            exit_status = msg_all(args);
        }
        return exit_status;
    }

    if (args.ips != NULL)
    {
        for (int i = 0; i <= args.repeat; i++)
        {
            exit_status = use_ips(args);
        }
        return exit_status;
    }

    // load the device configs into memory
    struct stat fstat;
    if ((stat(WIZ_PATH, &fstat)) < 0)
    {
        fprintf(stderr, "unable to stat device configuration file\n");
        perror(NULL);
        return EXIT_FAILURE;
    }
    if (fstat.st_size == 0)
    {
        fprintf(stderr, "device configuration file is empty\n");
        return EXIT_FAILURE;
    }
    char *buf = malloc(fstat.st_size + 1);

    FILE *fp = fopen(WIZ_PATH, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "unable to open device configuration file\n");
        perror(NULL);
        exit_status = EXIT_FAILURE;
        goto end;
    }
    if (fread(buf, fstat.st_size, 1, fp) == 0)
    {
        fprintf(stderr, "unable to read device configuration file\n");
        exit_status = EXIT_FAILURE;
        goto end;
    }
    if (fclose(fp) < 0)
    {
        fprintf(stderr, "unable to close device configuration file\n");
        perror(NULL);
        exit_status = EXIT_FAILURE;
        goto end;
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
    if (n == 0)
    {
        fprintf(stderr, "no devices read from the configuration file\n");
        exit_status = EXIT_FAILURE;
        goto end;
    }

   
    char msg[MAX_REQ];
    int mlen = json_msg(msg, args);
    if (mlen < 0) {
        fprintf(stderr, "error writing json message: %s\n", msg);
        goto end;
    }

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

    for (int i = 0; i <= args.repeat; i++)
    {
        if (send_cmds(msg, mlen, devs, n) < 0)
        {
            fprintf(stderr, "error sending cmds\n");
            exit_status = EXIT_FAILURE;
        }
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
    case 't':
        arg_info->repeat = atoi(arg);
        break;
    case 'u':
        arg_info->dimming = clamp(0, 100, atoi(arg)) + 1;
        break;
    case 'v':
        arg_info->speed = clamp(10, 200, atoi(arg));
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

int parse_ips(char *src, device devs[])
{
    int n = 0;
    int i = 0;
    int src_len = strlen(src);
    device new = {};
    for (; i < src_len && n < MAX_DEVS; n++)
    {
        devs[n] = new;
        devs[n].ip = &src[i];
        while (i < src_len)
        {
            if (src[i] == ',')
            {
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

int msg_all(struct arg_vals a)
{
    char buf[MAX_REQ];
    int n = json_msg(buf, a);
    return broadcast_udp(buf, n);
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

int use_ips(struct arg_vals args)
{
    device devs[MAX_DEVS];
    int n = parse_ips(args.ips, devs);
    if (n < 1)
    {
        fprintf(stderr, "unable to parse ip addresses");
        return EXIT_FAILURE;
    }
    char msg[MAX_REQ];
    int mlen = json_msg(msg, args);
    return send_cmds(msg, mlen, devs, n);
};


int json_msg(char *buf, struct arg_vals args) {
    // setState cmds
    if (args.turn_on) { 
        strcpy(buf, ON);
        return sizeof(ON);
    }
    if (args.turn_off) { 
        strcpy(buf, OFF);
        return sizeof(OFF);
    }

    char col_str[64] = "";
    char k_str[64] = "";
    char scene_str[64] = "";

    // set exclusive pilot cmds
    if (args.change_col) {sprintf(col_str, "\"r\":%u,\"g\":%u,\"b\":%u,", args.col.r, args.col.g, args.col.b);}
    else if (args.kelvin){sprintf(k_str, "\"temp\":%d,", args.kelvin);}
    else if (args.scene){sprintf(scene_str, "\"sceneId\":%d,", args.scene);}
    
    char dimming_str[64] = "";
    char speed_str[64] = "";
    // set additional parameters
    if (args.dimming){sprintf(dimming_str, "\"dimming\":%d,", args.dimming-1);}
    if (args.speed){sprintf(speed_str, "\"speed\":%d,", args.speed);};

    sprintf(buf, "{\"id\":1,\"method\":\"setPilot\",\"params\":{%s%s%s%s%s", col_str, k_str, scene_str, dimming_str, speed_str);

    int buf_len = strlen(buf);
    if (buf[buf_len-1] == '{') { return -1; }

    strcpy(&buf[buf_len-1], "}}");
    return buf_len + 1;
}