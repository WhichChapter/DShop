#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <vita2d.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define SCREEN_W 960
#define SCREEN_H 544
#define CATALOG_URL "https://raw.githubusercontent.com/WhichChapter/DShop/main/catalog/catalog.json"
#define MAX_CATALOG_BYTES (64 * 1024)
#define MAX_APPS 48
#define TOKEN_COUNT 4096

typedef struct {
    char id[64], title[96], description[256], category[48];
    char version[24], price[24], vpk_url[256], homepage[256];
} StoreApp;

typedef struct {
    char *data;
    size_t length;
    int failed;
} HttpBuffer;

typedef struct {
    int start, end, next, children, type;
} JsonToken;

enum { JSON_OBJECT = 1, JSON_ARRAY, JSON_STRING, JSON_PRIMITIVE };

static StoreApp apps[MAX_APPS];
static int app_count = 0;
static char store_status[128] = "Catalogue not loaded";
static char selected_url[256] = "";
static int selected_app = 0;

static void draw_text(vita2d_pgf *font, float x, float y, unsigned int colour, const char *text)
{
    vita2d_pgf_draw_text(font, x, y, colour, 1.0f, text);
}

static size_t receive_data(void *ptr, size_t size, size_t count, void *userdata)
{
    HttpBuffer *buffer = (HttpBuffer *)userdata;
    size_t bytes = size * count;
    if (bytes > MAX_CATALOG_BYTES - buffer->length) {
        buffer->failed = 1;
        return 0;
    }
    memcpy(buffer->data + buffer->length, ptr, bytes);
    buffer->length += bytes;
    buffer->data[buffer->length] = '\0';
    return bytes;
}

static int json_space(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static int parse_json_value(const char *json, int *position, JsonToken *tokens, int *used)
{
    while (json_space(json[*position])) (*position)++;
    if (!json[*position] || *used >= TOKEN_COUNT) return -1;

    int index = (*used)++;
    JsonToken *token = &tokens[index];
    token->start = *position;
    token->children = 0;

    char c = json[*position];
    if (c == '{' || c == '[') {
        token->type = (c == '{') ? JSON_OBJECT : JSON_ARRAY;
        char closing = (c == '{') ? '}' : ']';
        (*position)++;
        while (1) {
            while (json_space(json[*position]) || json[*position] == ',' || json[*position] == ':') (*position)++;
            if (json[*position] == closing) {
                (*position)++;
                break;
            }
            if (!json[*position]) return -1;
            if (parse_json_value(json, position, tokens, used) < 0) return -1;
            token->children++;
        }
    } else if (c == '"') {
        token->type = JSON_STRING;
        (*position)++;
        while (json[*position]) {
            if (json[*position] == '\\') {
                (*position) += 2;
                continue;
            }
            if (json[*position] == '"') {
                (*position)++;
                break;
            }
            (*position)++;
        }
    } else {
        token->type = JSON_PRIMITIVE;
        while (json[*position] && json[*position] != ',' && json[*position] != ']' &&
               json[*position] != '}' && !json_space(json[*position])) (*position)++;
    }
    token->end = *position;
    token->next = *used;
    return index;
}

static void token_string(const char *json, const JsonToken *token, char *out, size_t out_size)
{
    size_t n = 0;
    int start = token->start + (token->type == JSON_STRING ? 1 : 0);
    int end = token->end - (token->type == JSON_STRING ? 1 : 0);
    for (int i = start; i < end && n + 1 < out_size; i++) {
        char c = json[i];
        if (c == '\\' && i + 1 < end) {
            c = json[++i];
            if (c == 'n') c = ' ';
            else if (c == 'r' || c == 't') c = ' ';
            else if (c == 'u') {
                if (i + 4 < end) i += 4;
                c = '?';
            }
        }
        out[n++] = c;
    }
    out[n] = '\0';
}

static int object_value(const char *json, JsonToken *tokens, int object, const char *wanted)
{
    int child = object + 1;
    while (child < tokens[object].next) {
        int value = tokens[child].next;
        char key[96];
        token_string(json, &tokens[child], key, sizeof(key));
        if (strcmp(key, wanted) == 0) return value;
        if (value >= tokens[object].next) break;
        child = tokens[value].next;
    }
    return -1;
}

static void read_field(const char *json, JsonToken *tokens, int object, const char *key,
                       char *destination, size_t size)
{
    int value = object_value(json, tokens, object, key);
    if (value >= 0 && tokens[value].type == JSON_STRING)
        token_string(json, &tokens[value], destination, size);
}

static int parse_catalogue(const char *json)
{
    JsonToken *tokens = (JsonToken *)calloc(TOKEN_COUNT, sizeof(JsonToken));
    if (!tokens) return 0;
    int position = 0, used = 0;
    int root = parse_json_value(json, &position, tokens, &used);
    if (root < 0 || tokens[root].type != JSON_OBJECT) {
        free(tokens);
        return 0;
    }

    int array = object_value(json, tokens, root, "apps");
    if (array < 0 || tokens[array].type != JSON_ARRAY) {
        free(tokens);
        return 0;
    }

    app_count = 0;
    int item = array + 1;
    while (item < tokens[array].next && app_count < MAX_APPS) {
        if (tokens[item].type == JSON_OBJECT) {
            StoreApp *app = &apps[app_count];
            memset(app, 0, sizeof(*app));
            read_field(json, tokens, item, "id", app->id, sizeof(app->id));
            read_field(json, tokens, item, "title", app->title, sizeof(app->title));
            read_field(json, tokens, item, "description", app->description, sizeof(app->description));
            read_field(json, tokens, item, "category", app->category, sizeof(app->category));
            read_field(json, tokens, item, "version", app->version, sizeof(app->version));
            read_field(json, tokens, item, "price", app->price, sizeof(app->price));
            read_field(json, tokens, item, "vpk_url", app->vpk_url, sizeof(app->vpk_url));
            read_field(json, tokens, item, "homepage", app->homepage, sizeof(app->homepage));
            if (app->title[0] && app->vpk_url[0]) app_count++;
        }
        item = tokens[item].next;
    }
    free(tokens);
    return 1;
}

static int load_catalogue(void)
{
    char *body = (char *)malloc(MAX_CATALOG_BYTES + 1);
    if (!body) {
        snprintf(store_status, sizeof(store_status), "Not enough memory");
        return 0;
    }
    HttpBuffer buffer = { body, 0, 0 };
    body[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(store_status, sizeof(store_status), "Could not start HTTPS client");
        free(body);
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, CATALOG_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DShop-PSVita/0.2");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    CURLcode result = curl_easy_perform(curl);
    long response = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || response != 200 || buffer.failed) {
        snprintf(store_status, sizeof(store_status), "Online fetch failed (%ld / %d)", response, (int)result);
        free(body);
        return 0;
    }

    if (!parse_catalogue(body)) {
        snprintf(store_status, sizeof(store_status), "Catalogue JSON is invalid");
        free(body);
        return 0;
    }

    snprintf(store_status, sizeof(store_status), "Online catalogue loaded: %d apps", app_count);
    free(body);
    return 1;
}

static void network_start(void)
{
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    SceNetInitParam param;
    param.memory = malloc(4 * 1024 * 1024);
    param.size = 4 * 1024 * 1024;
    param.flags = 0;
    if (param.memory) sceNetInit(&param);
    sceNetCtlInit();
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

static void network_stop(void)
{
    curl_global_cleanup();
    sceNetCtlTerm();
    sceNetTerm();
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

int main(void)
{
    SceCtrlData pad;
    unsigned int previous_buttons = 0;
    int selected_tab = 0;
    const char *tabs[] = { "Discover", "Categories", "Downloads", "Library" };
    const int tab_count = 4;

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    network_start();

    if (vita2d_init() < 0) {
        network_stop();
        sceKernelExitProcess(1);
        return 1;
    }

    vita2d_pgf *font = vita2d_load_default_pgf();
    if (!font) {
        vita2d_fini();
        network_stop();
        sceKernelExitProcess(1);
        return 1;
    }

    load_catalogue();

    while (1) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed = pad.buttons & ~previous_buttons;
        previous_buttons = pad.buttons;

        if (pressed & SCE_CTRL_LEFT) selected_tab = (selected_tab + tab_count - 1) % tab_count;
        if (pressed & SCE_CTRL_RIGHT) selected_tab = (selected_tab + 1) % tab_count;
        if (pressed & SCE_CTRL_UP && app_count > 0) selected_app = (selected_app + app_count - 1) % app_count;
        if (pressed & SCE_CTRL_DOWN && app_count > 0) selected_app = (selected_app + 1) % app_count;
        if (pressed & SCE_CTRL_TRIANGLE) load_catalogue();
        if (pressed & SCE_CTRL_CIRCLE) {
            if (app_count > 0) {
                snprintf(selected_url, sizeof(selected_url), "%s", apps[selected_app].vpk_url);
            }
        }
        if (pressed & SCE_CTRL_START) break;

        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_rectangle(0, 0, SCREEN_W, 74, RGBA8(17, 22, 36, 255));
        vita2d_draw_rectangle(0, 74, SCREEN_W, 3, RGBA8(70, 150, 255, 255));
        draw_text(font, 38, 48, RGBA8(255, 255, 255, 255), "DSHOP");
        draw_text(font, 830, 45, RGBA8(145, 165, 195, 255), "PS VITA");

        for (int i = 0; i < tab_count; i++) {
            unsigned int colour = (i == selected_tab) ? RGBA8(90, 175, 255, 255) : RGBA8(175, 185, 205, 255);
            draw_text(font, 42 + (i * 220), 116, colour, tabs[i]);
        }
        vita2d_draw_rectangle(36, 137, SCREEN_W - 72, 2, RGBA8(55, 68, 90, 255));
        draw_text(font, 48, 183, RGBA8(255, 255, 255, 255), tabs[selected_tab]);

        if (selected_tab == 0 || selected_tab == 1) {
            draw_text(font, 48, 215, RGBA8(135, 155, 185, 255), store_status);
            if (app_count == 0) {
                draw_text(font, 48, 270, RGBA8(205, 210, 225, 255), "No listings yet. Add apps to catalog/catalog.json in the GitHub repo.");
            } else {
                int visible = app_count < 6 ? app_count : 6;
                int first = selected_app >= 6 ? selected_app - 5 : 0;
                for (int i = 0; i < visible; i++) {
                    int index = first + i;
                    unsigned int colour = (index == selected_app) ? RGBA8(90, 175, 255, 255) : RGBA8(210, 218, 232, 255);
                    char line[160];
                    snprintf(line, sizeof(line), "%s  |  %s  |  v%s", apps[index].title,
                             apps[index].category[0] ? apps[index].category : "Other",
                             apps[index].version[0] ? apps[index].version : "1.0");
                    draw_text(font, 55, 260 + i * 38, colour, line);
                }
                StoreApp *app = &apps[selected_app];
                draw_text(font, 55, 505, RGBA8(155, 180, 210, 255), app->description);
                if (app->price[0]) draw_text(font, 710, 505, RGBA8(120, 220, 160, 255), app->price);
                draw_text(font, 55, 475, RGBA8(130, 150, 180, 255), "CIRCLE: show VPK link");
                if (selected_url[0]) draw_text(font, 350, 475, RGBA8(255, 210, 120, 255), selected_url);
            }
        } else if (selected_tab == 2) {
            draw_text(font, 48, 245, RGBA8(175, 190, 215, 255), "Downloads will be added in the next store update.");
        } else {
            draw_text(font, 48, 245, RGBA8(175, 190, 215, 255), "Installed app tracking will be added in the next store update.");
        }

        draw_text(font, 42, 530, RGBA8(125, 140, 165, 255),
                  "LEFT/RIGHT: Tabs   UP/DOWN: Select   TRIANGLE: Refresh   CIRCLE: VPK link   START: Exit");
        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceDisplayWaitVblankStart();
    }

    vita2d_free_pgf(font);
    vita2d_fini();
    network_stop();
    sceKernelExitProcess(0);
    return 0;
}
