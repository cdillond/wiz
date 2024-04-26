#include <argp.h>
#include <string.h>
#include <stdbool.h>

#define WIZ_DATA_PATH "wiz.csv"

#define PORT 38899
#define MAX_DEVS 256
#define MAX_REQ 128

#define OFF "{\"id\":1,\"method\":\"setState\",\"params\":{\"state\":false}}"
#define ON "{\"id\":1,\"method\":\"setState\",\"params\":{\"state\":true}}"
#define COLOR "{\"id\":%d,\"method\":\"setPilot\",\"params\":{\"r\":%d,\"g\":%d,\"b\":%d}}"
#define KELVIN "{\"id\":%d,\"method\":\"setPilot\",\"params\":{\"temp\":%d}}"
#define SCENE "{\"id\":%d,\"method\":\"setPilot\",\"params\":{\"sceneId\":%d}}"
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

typedef struct color
{
    int r;
    int g;
    int b;
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

int parse_csv(char *data, int n, device devs[], char *search, char *search_room);

int init_color(color *, char *);

// Reads the file named by path and stores parsed devices in buf. Returns the number of devices parsed, or -1 if there was an error.
int read_devs(device buf[], char *path, char *look);

int op_str(char buf[], device dev);

scene str_scene(char *);

bool is_in(char *, char *);

int broadcast_udp(int, int);