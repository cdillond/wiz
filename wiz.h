#include <argp.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define WIZ_PATH "/.local/state/wiz/wiz.csv"
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
    OP_OFF = 1,
    OP_ON = 1 << 1,
    OP_COLOR = 1 << 2,
    OP_KELVIN = 1 << 3,
    OP_SCENE = 1 << 4,
} op_t;

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
    op_t op;
    scene scene;
} device;

// send_cmds opens a UDP socket and writes n cmds to it.
int send_cmds(device *, int);

struct arg_vals
{
    bool change_col;
    bool turn_off;
    bool turn_on;
    bool discover;
    bool use_ips;
    bool list;
    char *buf;
    char *name;
    char *room;
    color col;
    int kelvin;
    int seconds;
    int num_devs;
    scene scene;
};

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

// broadcast_udp broadcasts a message via UDP on port 38899 to the local network. It then prints the IP address of incoming responses either until timeout (in seconds) has elapsed or max_resps responses have been received.
int broadcast_udp(int timeout, int max_resps);
