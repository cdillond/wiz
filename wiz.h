#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define WIZ_PATH "/usr/share/wiz/wiz.csv"
#define PORT 38899
#define MAX_DEVS 256
#define MAX_REQ 128

#define OFF "{\"id\":1,\"method\":\"setState\",\"params\":{\"state\":false}}"
#define ON "{\"id\":1,\"method\":\"setState\",\"params\":{\"state\":true}}"
#define COLOR "{\"id\":1,\"method\":\"setPilot\",\"params\":{\"r\":%u,\"g\":%u,\"b\":%u}}"
#define KELVIN "{\"id\":1,\"method\":\"setPilot\",\"params\":{\"temp\":%d}}"
#define SCENE "{\"id\":1,\"method\":\"setPilot\",\"params\":{\"sceneId\":%d}}"
#define INFO "{\"id\":-2147483648,\"method\":\"getDevInfo\"}"

typedef enum
{
    CMD_OFF = 1,
    CMD_ON = 1 << 1,
    CMD_COLOR = 1 << 2,
    CMD_KELVIN = 1 << 3,
    CMD_SCENE = 1 << 4,
} cmd;

typedef enum scene
{
    BAD_SCENE,
    OCEAN,
    ROMANCE,
    SUNSET,
    PARTY,
    FIREPLACE,
    COZY,
    FOREST,
    PASTEL_COLORS,
    WAKE_UP,
    BEDTIME,
    WARM_WHITE,
    DAYLIGHT,
    COOL_WHITE,
    NIGHT_LIGHT,
    FOCUS,
    RELAX,
    TRUE_COLORS,
    TV_TIME,
    PLANT_GROWTH,
    SPRING,
    SUMMER,
    FALL,
    DEEP_DIVE,
    JUNGLE,
    MOJITO,
    CLUB,
    CHRISTMAS,
    HALLOWEEN,
    CANDLE_LIGHT,
    GOLDEN_WHITE,
    PULSE,
    STEAMPUNK,
    DIWALI,
    MAX_SCENE,
} scene;

/*
  A color contains the r, g, and b values for the light color, each of which must be in [0, 255).
 */
typedef struct color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color;

typedef struct device
{
    char *ip;
    char *name;
    char *room;
    color col;
    int kelvin;
    cmd op;
    scene scene;
} device;

struct arg_vals
{
    bool broadcast;
    bool change_col;
    bool turn_off;
    bool turn_on;
    bool discover;
    bool list;
    char *name;
    char *room;
    char *ips;
    color col;
    int kelvin;
    int seconds;
    int num_devs;
    int repeat;
    scene scene;
};

// send_cmds opens a UDP socket and writes n cmds to it.
int send_cmds(char *msg, int mlen, device devs[], int num_devs);

// parse_csv interprets data as the contents of a csv file. It writes the information to devs, which should have a length of MAX_DEVS. If search and/or search_room are not NULL, parse_csv ignores all devices with names or room names that do not mach these strings. (Each string argument is interpreted either as a single name or as a comma-separated list of names.) parse_csv returns the number of devices loaded into devs, or -1 on failure.
int parse_csv(char *data, int n, device devs[], char *search, char *search_room);

// init_color parses the string argument as either a named color or a comma-separated list of r, g, and b values of a color. It updates the color and returns 0 on success or -1 on failure.
int init_color(color *col, char *s);

// req_msg builds a request body that represents dev and writes it to buf.
int req_msg(char buf[], device dev);

// scene_str returns the scene id that matches s.
scene str_scene(char *s);

// is_in interprets list as either a single string or a comma-separated list of strings. It returns true if s is equal to any of those strings.
bool is_in(char *s, char *list);

// broadcast_udp_wait broadcasts a message via UDP on port 38899 to the local network. It then prints the IP address of incoming responses either until timeout (in seconds) has elapsed or max_resps responses have been received.
int broadcast_udp_wait(char *msg, int mlen, int timeout, int max_resps);

// msg_all  broadcasts a command based on the supplied arguments to all devices on the current network.
int msg_all(struct arg_vals a);

// init_dev returns a device reflecting the values from the supplied args. This function validates the arg_vals to ensure the returned device will be acceptable.
device init_dev(struct arg_vals args);
// update_dev is similar to init_dev, but it updates an existing dev to reflect the supplied args.
void update_dev(device *dev, struct arg_vals args);
// broadcast_udp broadcasts the msg to all devices on the current network. It does not wait for any responses.
int broadcast_udp(char *msg, int mlen);
// parse_ips parses a comma-separated list of ipv4 addresses and returns the number of updated devices.
int parse_ips(char *src, device devs[]);
int use_ips(struct arg_vals args);
