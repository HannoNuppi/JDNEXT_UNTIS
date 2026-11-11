#include <switch.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>
#include "ui.h"

/*
 * Untis Next native Nintendo Switch client.
 * No WebKit/Web applet is used. Network access is through HTTPS + Firebase
 * REST APIs; admin mutations go through existing authenticated callable
 * Cloud Functions.
 */

#define TOBI_PROJECT "tobiservices"
#define OLD_PROJECT "next-untis-plus"
#define TOBI_API_KEY "AIzaSyCwVh8-JIm-pj8N8bOWgwz8-G_cLQ6AVuQ"
#define OLD_API_KEY "AIzaSyBgkWUtFIH63yJUw4zTbNtplKL5HqDDXMg"
#define MAX_JSON_TOKENS 12000
#define MAX_ENTRIES 24
#define HTTP_LIMIT (768 * 1024)

typedef struct {
    int start, end, size, parent;
    char type;
} JsonToken;

typedef struct {
    char *data;
    size_t size;
    long status;
} HttpResponse;

typedef struct {
    char idToken[4096];
    char refreshToken[2048];
    char uid[160];
    char email[160];
    char displayName[160];
    char tags[512];
    char ownedThemes[768];
    char equippedTheme[64];
    char profileTitle[64];
    char profileIcon[32];
    int goldCoins;
    bool disabled;
    bool signedIn;
    bool isAdmin;
} Account;

typedef struct {
    const char *id;
    const char *name;
    int price;
    bool event_only;
    bool local_only;
} ThemeOption;

typedef struct {
    char id[128];
    char title[280];
    char author[96];
    int reportCount;
    bool classwork;
    bool legacy;
} LessonEntry;

typedef struct {
    char idToken[4096];
    char uid[160];
    bool valid;
} GuestSession;

typedef struct {
    char uid[160];
    char displayName[96];
    char email[160];
    char tags[512];
    int goldCoins;
    bool disabled;
} AdminUser;

typedef enum {
    SCREEN_TIMETABLE = 0,
    SCREEN_LESSON,
    SCREEN_ACCOUNT,
    SCREEN_ADMIN,
    SCREEN_THEMES,
    SCREEN_PROFILE
} Screen;

static Account g_account;
static GuestSession g_guestTobi;
static GuestSession g_guestOld;
static AdminUser g_admin_users[24];
static int g_admin_user_count = 0;
static unsigned long g_write_counter = 0;
static bool g_socket_initialized = false;
static bool g_maintenance = false;
static char g_maintenance_message[500];
static char g_status[220] = "Bereit.";
static struct tm g_week_monday;
static int g_day = 0;
static int g_period = 0;
static int g_menu_selection = 0;
static int g_profile_title_selection = 0;
static int g_profile_icon_selection = 0;
static Screen g_screen = SCREEN_TIMETABLE;
static LessonEntry g_entries[MAX_ENTRIES];
static int g_entry_count = 0;

static const char *DAY_NAMES[5] = {
    "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag"
};
static const char *DAY_SHORT[5] = { "Mo", "Di", "Mi", "Do", "Fr" };
static const char *PERIOD_LABELS[6] = { "1.", "2.", "3.", "4.", "5.", "6." };
static const char *PERIOD_TIMES[6] = {
    "07:50-08:50", "08:55-09:55", "10:15-11:15",
    "11:20-12:20", "13:00-14:00", "14:05-15:05"
};

static const char *SCHEDULE_A[5][6] = {
    {"Deutsch", "Geschichte", "Erdkunde", "Latein", "", ""},
    {"Wahlpflicht", "Kunst", "Physik", "Mathe", "", ""},
    {"Religion/PPL", "Sport", "Chemie", "Englisch", "", ""},
    {"Mathe", "Deutsch", "Latein", "Biologie", "", ""},
    {"Wahlpflicht", "Englisch", "Chemie", "Politik", "", ""}
};
static const char *SCHEDULE_B[5][6] = {
    {"Deutsch", "Geschichte", "Erdkunde", "Latein", "", ""},
    {"Wahlpflicht", "Kunst", "Physik", "Mathe", "", ""},
    {"Religion/PPL", "Sport", "Mathe", "Englisch", "", ""},
    {"Mathe", "Deutsch", "Latein", "Biologie", "", ""},
    {"Wahlpflicht", "Englisch", "Chemie", "Politik", "", ""}
};

static const ThemeOption THEME_OPTIONS[] = {
    {"Next","Next",0,false,false},
    {"Ocean","Ocean Pulse",10,false,false},
    {"Sunset","Sunset Rush",10,false,false},
    {"Forest","Forest Beat",10,false,false},
    {"Candy","Candy Pop",10,false,false},
    {"Midnight","Midnight Club",10,false,false},
    {"Aurora","Aurora Glow",10,false,false},
    {"Retro","Retro Arcade",10,false,false},
    {"RoyalRebels","Royal Rebels",50,false,false},
    {"Bronze","Bronze",50,false,false},
    {"Silver","Silber",70,false,false},
    {"Gold","Gold",100,false,false},
    {"WinterEvent","Winterwunder",0,true,false},
    {"BirthdayEvent","Birthday Bash",0,true,false},
    {"HalloweenEvent","Halloween Afterdark",0,true,false},
    {"WinterGoldEvent","Winter Gold",0,true,false},
    {"AdminMatrix","Admin Matrix",0,true,true}
};
static const char *PROFILE_TITLE_IDS[] = {
    "", "Newcomer", "Dance Fan", "Gold Collector", "Theme Hunter", "Night Owl", "Legend"
};
static const char *PROFILE_TITLE_LABELS[] = {
    "KEIN TITEL", "NEWCOMER", "DANCE FAN", "GOLD COLLECTOR", "THEME HUNTER", "NIGHT OWL", "LEGEND"
};
static const char *PROFILE_ICON_IDS[] = {
    "", "🎮", "🦊", "🐱", "🐸", "🐼", "🐉", "🦄", "👾", "🎃", "⭐", "💎", "🌈", "🪩"
};
static const char *PROFILE_ICON_LABELS[] = {
    "KEIN ICON", "GAMEPAD", "FUCHS", "KATZE", "FROSCH", "PANDA", "DRACHE",
    "EINHORN", "ALIEN", "KUERBIS", "STERN", "DIAMANT", "REGENBOGEN", "DISCO"
};

/* --------------------------- Small JSON parser ------------------------- */
static void json_skip_ws(const char *s, int *p) {
    while (s[*p] && (s[*p] == ' ' || s[*p] == '\n' || s[*p] == '\r' || s[*p] == '\t')) (*p)++;
}

static int json_new_token(JsonToken *t, int *n, char type, int start, int parent) {
    if (*n >= MAX_JSON_TOKENS) return -1;
    int id = (*n)++;
    t[id].start = start;
    t[id].end = start;
    t[id].size = 0;
    t[id].parent = parent;
    t[id].type = type;
    return id;
}

static int json_parse_value(const char *s, int *p, JsonToken *t, int *n, int parent);

static int json_parse_string(const char *s, int *p, JsonToken *t, int *n, int parent) {
    if (s[*p] != '"') return -1;
    int id = json_new_token(t, n, 's', *p + 1, parent);
    if (id < 0) return -1;
    (*p)++;
    while (s[*p]) {
        if (s[*p] == '\\') {
            (*p)++;
            if (s[*p]) (*p)++;
            continue;
        }
        if (s[*p] == '"') {
            t[id].end = *p;
            (*p)++;
            if (parent >= 0) t[parent].size++;
            return id;
        }
        (*p)++;
    }
    return -1;
}

static int json_parse_value(const char *s, int *p, JsonToken *t, int *n, int parent) {
    json_skip_ws(s, p);
    char c = s[*p];
    if (!c) return -1;

    if (c == '"') return json_parse_string(s, p, t, n, parent);

    if (c == '{' || c == '[') {
        char type = c == '{' ? 'o' : 'a';
        int id = json_new_token(t, n, type, *p, parent);
        if (id < 0) return -1;
        (*p)++;
        json_skip_ws(s, p);
        char close = c == '{' ? '}' : ']';
        while (s[*p] && s[*p] != close) {
            if (s[*p] == ',' || s[*p] == ':') {
                (*p)++;
                continue;
            }
            int child = (c == '{' && s[*p] == '"')
                ? json_parse_string(s, p, t, n, id)
                : json_parse_value(s, p, t, n, id);
            if (child < 0) return -1;
            json_skip_ws(s, p);
        }
        if (s[*p] != close) return -1;
        (*p)++;
        t[id].end = *p;
        return id;
    }

    int id = json_new_token(t, n, 'p', *p, parent);
    if (id < 0) return -1;
    while (s[*p] && s[*p] != ',' && s[*p] != '}' && s[*p] != ']' &&
           s[*p] != ' ' && s[*p] != '\n' && s[*p] != '\r' && s[*p] != '\t') {
        (*p)++;
    }
    t[id].end = *p;
    if (parent >= 0) t[parent].size++;
    return id;
}

static int json_parse(const char *s, JsonToken *t, int *count) {
    int p = 0;
    *count = 0;
    json_skip_ws(s, &p);
    int root = json_parse_value(s, &p, t, count, -1);
    return root >= 0 ? root : -1;
}

static int json_subtree_end(JsonToken *t, int count, int id) {
    int j = id + 1;
    while (j < count) {
        int parent = t[j].parent;
        bool descendant = false;
        while (parent >= 0) {
            if (parent == id) { descendant = true; break; }
            parent = t[parent].parent;
        }
        if (!descendant) break;
        j++;
    }
    return j;
}

static bool json_token_equals(const char *s, JsonToken *t, int id, const char *value) {
    if (id < 0) return false;
    size_t n = (size_t)(t[id].end - t[id].start);
    return strlen(value) == n && strncmp(s + t[id].start, value, n) == 0;
}

static int json_object_get(const char *s, JsonToken *t, int count, int object, const char *key) {
    if (object < 0 || t[object].type != 'o') return -1;
    int k = object + 1;
    while (k < count && t[k].parent == object) {
        int v = k + 1;
        if (v >= count || t[v].parent != object) return -1;
        if (json_token_equals(s, t, k, key)) return v;
        k = json_subtree_end(t, count, v);
    }
    return -1;
}

static int json_array_get(JsonToken *t, int count, int array, int wanted) {
    if (array < 0 || t[array].type != 'a' || wanted < 0) return -1;
    int item = array + 1;
    int index = 0;
    while (item < count && t[item].parent == array) {
        if (index == wanted) return item;
        item = json_subtree_end(t, count, item);
        index++;
    }
    return -1;
}

static void json_copy_string(const char *s, JsonToken *t, int id, char *out, size_t cap) {
    if (!cap) return;
    out[0] = '\0';
    if (id < 0 || t[id].type != 's') return;
    size_t w = 0;
    for (int i = t[id].start; i < t[id].end && w + 1 < cap; i++) {
        char c = s[i];
        if (c == '\\' && i + 1 < t[id].end) {
            char e = s[++i];
            if (e == 'n') c = '\n';
            else if (e == 'r') c = '\r';
            else if (e == 't') c = '\t';
            else if (e == '"' || e == '\\' || e == '/') c = e;
            else if (e == 'u' && i + 4 < t[id].end) {
                /* ASCII is enough for labels; keep a safe placeholder for escaped Unicode. */
                c = '?';
                i += 4;
            } else c = e;
        }
        out[w++] = c;
    }
    out[w] = '\0';
}

static void json_get_string(const char *s, JsonToken *t, int n, int obj,
                            const char *key, char *out, size_t cap) {
    int value = json_object_get(s, t, n, obj, key);
    json_copy_string(s, t, value, out, cap);
}

static void firestore_field_string(const char *s, JsonToken *t, int n, int fields,
                                   const char *field, char *out, size_t cap) {
    int wrapper = json_object_get(s, t, n, fields, field);
    int value = json_object_get(s, t, n, wrapper, "stringValue");
    json_copy_string(s, t, value, out, cap);
}

static bool firestore_field_bool(const char *s, JsonToken *t, int n, int fields, const char *field) {
    int wrapper = json_object_get(s, t, n, fields, field);
    int value = json_object_get(s, t, n, wrapper, "booleanValue");
    return value >= 0 && t[value].type == 'p' &&
           strncmp(s + t[value].start, "true", 4) == 0;
}

static int firestore_field_int(const char *s, JsonToken *t, int n, int fields, const char *field) {
    int wrapper = json_object_get(s, t, n, fields, field);
    int value = json_object_get(s, t, n, wrapper, "integerValue");
    if (value < 0) return 0;
    char tmp[32];
    size_t len = (size_t)(t[value].end - t[value].start);
    if (!len || len >= sizeof(tmp)) return 0;
    memcpy(tmp, s + t[value].start, len);
    tmp[len] = 0;
    return atoi(tmp);
}

static void firestore_field_string_array(const char *s, JsonToken *t, int n,
                                          int fields, const char *field,
                                          char *out, size_t cap) {
    if (!cap) return;
    out[0] = '|';
    out[1] = 0;
    int wrapper = json_object_get(s, t, n, fields, field);
    int array = json_object_get(s, t, n, wrapper, "arrayValue");
    int values = json_object_get(s, t, n, array, "values");
    if (values < 0 || t[values].type != 'a') return;
    for (int i = 0; i < 30; i++) {
        int entry = json_array_get(t, n, values, i);
        if (entry < 0) break;
        int value = json_object_get(s, t, n, entry, "stringValue");
        char item[80] = "";
        json_copy_string(s, t, value, item, sizeof(item));
        if (!item[0] || strchr(item, '|')) continue;
        size_t used = strlen(out);
        if (used + strlen(item) + 2 >= cap) break;
        strncat(out, item, cap - used - 1);
        strncat(out, "|", cap - strlen(out) - 1);
    }
}

static bool account_owns_theme(const char *theme_id) {
    if (!theme_id || !theme_id[0]) return false;
    char needle[96];
    snprintf(needle, sizeof(needle), "|%s|", theme_id);
    return strstr(g_account.ownedThemes, needle) != NULL;
}

static bool theme_is_visible(const ThemeOption *theme) {
    if (!theme) return false;
    if (theme->local_only) return g_account.isAdmin;
    if (theme->event_only) return account_owns_theme(theme->id);
    return true;
}

static int visible_theme_count(void) {
    int count = 0;
    for (size_t i = 0; i < sizeof(THEME_OPTIONS) / sizeof(THEME_OPTIONS[0]); i++) {
        if (theme_is_visible(&THEME_OPTIONS[i])) count++;
    }
    return count;
}

static const ThemeOption *visible_theme_at(int wanted) {
    int index = 0;
    for (size_t i = 0; i < sizeof(THEME_OPTIONS) / sizeof(THEME_OPTIONS[0]); i++) {
        if (!theme_is_visible(&THEME_OPTIONS[i])) continue;
        if (index++ == wanted) return &THEME_OPTIONS[i];
    }
    return NULL;
}

static int profile_title_index(void) {
    for (int i = 0; i < 7; i++)
        if (strcmp(g_account.profileTitle, PROFILE_TITLE_IDS[i]) == 0) return i;
    return 0;
}

static int profile_icon_index(void) {
    for (int i = 0; i < 14; i++)
        if (strcmp(g_account.profileIcon, PROFILE_ICON_IDS[i]) == 0) return i;
    return 0;
}

/* ------------------------------- HTTPS --------------------------------- */
static size_t http_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    HttpResponse *r = (HttpResponse *)userdata;
    size_t bytes = size * nmemb;
    if (r->size + bytes > HTTP_LIMIT) return 0;
    char *next = (char *)realloc(r->data, r->size + bytes + 1);
    if (!next) return 0;
    r->data = next;
    memcpy(r->data + r->size, ptr, bytes);
    r->size += bytes;
    r->data[r->size] = '\0';
    return bytes;
}

static bool http_request(const char *url, const char *method, const char *body,
                         const char *bearer, HttpResponse *out) {
    memset(out, 0, sizeof(*out));
    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(g_status, sizeof(g_status), "HTTPS-Client konnte nicht initialisiert werden.");
        return false;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (body) headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[4300];
    if (bearer && bearer[0]) {
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", bearer);
        headers = curl_slist_append(headers, auth_header);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "UntisNext-Switch/1.1");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body ? (long)strlen(body) : 0L);
    } else if (strcmp(method, "PATCH") == 0 || strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
        if (body) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    }

    CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out->status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        snprintf(g_status, sizeof(g_status), "Netzwerkfehler: %s", curl_easy_strerror(result));
        free(out->data);
        out->data = NULL;
        out->size = 0;
        return false;
    }
    if (!out->data) {
        out->data = (char *)calloc(1, 1);
        if (!out->data) return false;
    }
    return out->status >= 200 && out->status < 300;
}

static void http_response_free(HttpResponse *r) {
    free(r->data);
    memset(r, 0, sizeof(*r));
}

static void json_escape(const char *in, char *out, size_t cap) {
    size_t w = 0;
    if (!cap) return;
    for (size_t i = 0; in[i] && w + 3 < cap; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            out[w++] = '\\';
            out[w++] = (char)c;
        } else if (c == '\n') {
            out[w++] = '\\'; out[w++] = 'n';
        } else if (c == '\r') {
            out[w++] = '\\'; out[w++] = 'r';
        } else if (c < 0x20) {
            continue;
        } else out[w++] = (char)c;
    }
    out[w] = 0;
}

static bool firebase_sign_in(const char *email, const char *password) {
    char safe_email[400], safe_password[600], body[1100], url[300];
    json_escape(email, safe_email, sizeof(safe_email));
    json_escape(password, safe_password, sizeof(safe_password));
    snprintf(url, sizeof(url),
        "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=%s",
        TOBI_API_KEY);
    snprintf(body, sizeof(body),
        "{\"email\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}",
        safe_email, safe_password);

    HttpResponse r;
    if (!http_request(url, "POST", body, NULL, &r)) {
        snprintf(g_status, sizeof(g_status), "Anmeldung fehlgeschlagen (HTTP %ld).", r.status);
        http_response_free(&r);
        return false;
    }

    JsonToken *t = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    int n = 0, root = json_parse(r.data, t, &n);
    if (root < 0) {
        snprintf(g_status, sizeof(g_status), "Firebase-Antwort konnte nicht gelesen werden.");
        free(t); http_response_free(&r); return false;
    }

    json_get_string(r.data, t, n, root, "idToken", g_account.idToken, sizeof(g_account.idToken));
    json_get_string(r.data, t, n, root, "refreshToken", g_account.refreshToken, sizeof(g_account.refreshToken));
    json_get_string(r.data, t, n, root, "localId", g_account.uid, sizeof(g_account.uid));
    json_get_string(r.data, t, n, root, "email", g_account.email, sizeof(g_account.email));
    json_get_string(r.data, t, n, root, "displayName", g_account.displayName, sizeof(g_account.displayName));
    g_account.signedIn = g_account.idToken[0] != 0 && g_account.uid[0] != 0;
    free(t);
    http_response_free(&r);

    if (!g_account.signedIn) {
        snprintf(g_status, sizeof(g_status), "Firebase hat kein gültiges Anmeldetoken geliefert.");
        return false;
    }
    return true;
}

static bool firestore_get(const char *project, const char *path, const char *token,
                          HttpResponse *out) {
    char url[1500];
    snprintf(url, sizeof(url),
        "https://firestore.googleapis.com/v1/projects/%s/databases/(default)/documents/%s",
        project, path);
    return http_request(url, "GET", NULL, token, out);
}

static bool firestore_list(const char *project, const char *path, const char *token,
                           HttpResponse *out) {
    char url[1500];
    snprintf(url, sizeof(url),
        "https://firestore.googleapis.com/v1/projects/%s/databases/(default)/documents/%s?pageSize=30",
        project, path);
    return http_request(url, "GET", NULL, token, out);
}

static bool firestore_commit(const char *project, const char *body,
                             const char *token, HttpResponse *out) {
    char url[500];
    snprintf(url, sizeof(url),
        "https://firestore.googleapis.com/v1/projects/%s/databases/(default)/documents:commit",
        project);
    return http_request(url, "POST", body, token, out);
}

static bool firebase_anonymous_session(const char *api_key, GuestSession *session) {
    if (session->valid && session->idToken[0] && session->uid[0]) return true;

    char url[300];
    snprintf(url, sizeof(url),
        "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=%s", api_key);

    HttpResponse r;
    if (!http_request(url, "POST", "{\"returnSecureToken\":true}", NULL, &r)) {
        snprintf(g_status, sizeof(g_status),
            "Gast-Anmeldung fehlgeschlagen (HTTP %ld).", r.status);
        http_response_free(&r);
        return false;
    }

    JsonToken *tokens = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    if (!tokens) {
        http_response_free(&r);
        snprintf(g_status, sizeof(g_status), "Nicht genug Speicher fuer die Gast-Anmeldung.");
        return false;
    }

    int count = 0;
    int root = json_parse(r.data, tokens, &count);
    if (root >= 0) {
        json_get_string(r.data, tokens, count, root, "idToken",
                        session->idToken, sizeof(session->idToken));
        json_get_string(r.data, tokens, count, root, "localId",
                        session->uid, sizeof(session->uid));
    }
    free(tokens);
    http_response_free(&r);

    session->valid = session->idToken[0] != 0 && session->uid[0] != 0;
    if (!session->valid) {
        memset(session, 0, sizeof(*session));
        snprintf(g_status, sizeof(g_status), "Firebase lieferte keine gueltige Gast-Sitzung.");
        return false;
    }
    return true;
}

static void make_document_id(char *out, size_t cap) {
    unsigned long now = (unsigned long)time(NULL);
    g_write_counter++;
    snprintf(out, cap, "sw-%lu-%lu", now, g_write_counter);
}

static bool save_classwork_entry(const char *hw_key, const char *title) {
    const char *clean = title ? title : "";
    size_t title_len = strlen(clean);
    if (title_len < 2 || title_len > 120) {
        snprintf(g_status, sizeof(g_status), "Die Bezeichnung muss 2 bis 120 Zeichen lang sein.");
        return false;
    }

    if (!firebase_anonymous_session(OLD_API_KEY, &g_guestOld)) return false;

    char safe_title[300], id[128], body[2600];
    json_escape(clean, safe_title, sizeof(safe_title));
    make_document_id(id, sizeof(id));

    int needed = snprintf(body, sizeof(body),
        "{\"writes\":[{"
          "\"update\":{"
            "\"name\":\"projects/%s/databases/(default)/documents/classwork/%s/entries/%s\","
            "\"fields\":{"
              "\"title\":{\"stringValue\":\"%s\"},"
              "\"createdBy\":{\"stringValue\":\"%s\"}"
            "}"
          "},"
          "\"updateTransforms\":[{\"fieldPath\":\"createdAt\","
            "\"setToServerValue\":\"REQUEST_TIME\"}],"
          "\"currentDocument\":{\"exists\":false}"
        "}]}",
        OLD_PROJECT, hw_key, id, safe_title, g_guestOld.uid);

    if (needed < 0 || (size_t)needed >= sizeof(body)) {
        snprintf(g_status, sizeof(g_status), "Klassenarbeit ist zu lang.");
        return false;
    }

    HttpResponse response;
    bool ok = firestore_commit(OLD_PROJECT, body, g_guestOld.idToken, &response);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "Klassenarbeit abgelehnt (HTTP %ld).", response.status);
    } else {
        snprintf(g_status, sizeof(g_status), "Klassenarbeit wurde eingetragen.");
    }
    http_response_free(&response);
    return ok;
}

static bool save_homework_entry(const char *hw_key, const char *text,
                                const char *author) {
    const char *clean_text = text ? text : "";
    const char *clean_author = author ? author : "";
    size_t text_len = strlen(clean_text), author_len = strlen(clean_author);
    if (text_len < 3 || text_len > 1000 || author_len > 80) {
        snprintf(g_status, sizeof(g_status),
            "Hausaufgabe: 3 bis 1000 Zeichen; Name maximal 80 Zeichen.");
        return false;
    }

    const char *token = NULL;
    const char *auth_uid = NULL;
    bool reward = false;
    if (g_account.signedIn) {
        token = g_account.idToken;
        auth_uid = g_account.uid;
        reward = !g_account.disabled && g_account.goldCoins > -50;
    } else {
        if (!firebase_anonymous_session(TOBI_API_KEY, &g_guestTobi)) return false;
        token = g_guestTobi.idToken;
        auth_uid = g_guestTobi.uid;
    }

    char safe_text[2100], safe_author[200], entry_id[128], body[14000];
    char event_id[256], owner_account_value[220];
    json_escape(clean_text, safe_text, sizeof(safe_text));
    json_escape(clean_author, safe_author, sizeof(safe_author));
    make_document_id(entry_id, sizeof(entry_id));
    snprintf(event_id, sizeof(event_id), "reward-%s-%s", hw_key, entry_id);
    if (g_account.signedIn) {
        snprintf(owner_account_value, sizeof(owner_account_value),
            "{\"stringValue\":\"%s\"}", auth_uid);
    } else {
        snprintf(owner_account_value, sizeof(owner_account_value), "{\"nullValue\":null}");
    }

    int needed;
    if (reward) {
        needed = snprintf(body, sizeof(body),
          "{\"writes\":["
            "{\"update\":{"
              "\"name\":\"projects/%s/databases/(default)/documents/jdnextHomework/%s/entries/%s\","
              "\"fields\":{"
                "\"text\":{\"stringValue\":\"%s\"},"
                "\"author\":{\"stringValue\":\"%s\"},"
                "\"images\":{\"arrayValue\":{}},"
                "\"reportCount\":{\"integerValue\":\"0\"}"
              "}"
            "},"
            "\"updateTransforms\":[{\"fieldPath\":\"createdAt\","
              "\"setToServerValue\":\"REQUEST_TIME\"}],"
            "\"currentDocument\":{\"exists\":false}"
            "},"
            "{\"update\":{"
              "\"name\":\"projects/%s/databases/(default)/documents/jdnextHomeworkPrivate/%s/entries/%s\","
              "\"fields\":{"
                "\"authorAuthUid\":{\"stringValue\":\"%s\"},"
                "\"authorTobiUid\":{\"stringValue\":\"%s\"},"
                "\"authorTobiAccountUid\":{\"stringValue\":\"%s\"}"
              "}"
            "},"
            "\"updateTransforms\":[{\"fieldPath\":\"createdAt\","
              "\"setToServerValue\":\"REQUEST_TIME\"}],"
            "\"currentDocument\":{\"exists\":false}"
            "},"
            "{\"update\":{"
              "\"name\":\"projects/%s/databases/(default)/documents/users/%s\","
              "\"fields\":{\"lastJdnextCoinEventId\":{\"stringValue\":\"%s\"}}"
            "},"
            "\"updateMask\":{\"fieldPaths\":[\"lastJdnextCoinEventId\"]},"
            "\"updateTransforms\":["
              "{\"fieldPath\":\"goldCoins\",\"increment\":{\"integerValue\":\"10\"}},"
              "{\"fieldPath\":\"lastProfileChangeAt\",\"setToServerValue\":\"REQUEST_TIME\"}"
            "],"
            "\"currentDocument\":{\"exists\":true}"
            "},"
            "{\"update\":{"
              "\"name\":\"projects/%s/databases/(default)/documents/jdnextCoinEvents/%s\","
              "\"fields\":{"
                "\"uid\":{\"stringValue\":\"%s\"},"
                "\"amount\":{\"integerValue\":\"10\"},"
                "\"type\":{\"stringValue\":\"homework-post\"},"
                "\"hwKey\":{\"stringValue\":\"%s\"},"
                "\"entryId\":{\"stringValue\":\"%s\"}"
              "}"
            "},"
            "\"updateTransforms\":[{\"fieldPath\":\"createdAt\","
              "\"setToServerValue\":\"REQUEST_TIME\"}],"
            "\"currentDocument\":{\"exists\":false}"
          "}"
          "]}",
          TOBI_PROJECT, hw_key, entry_id, safe_text, safe_author,
          TOBI_PROJECT, hw_key, entry_id, auth_uid, auth_uid, auth_uid,
          TOBI_PROJECT, auth_uid, event_id,
          TOBI_PROJECT, event_id, auth_uid, hw_key, entry_id);
    } else {
        needed = snprintf(body, sizeof(body),
          "{\"writes\":["
            "{\"update\":{"
              "\"name\":\"projects/%s/databases/(default)/documents/jdnextHomework/%s/entries/%s\","
              "\"fields\":{"
                "\"text\":{\"stringValue\":\"%s\"},"
                "\"author\":{\"stringValue\":\"%s\"},"
                "\"images\":{\"arrayValue\":{}},"
                "\"reportCount\":{\"integerValue\":\"0\"}"
              "}"
            "},"
            "\"updateTransforms\":[{\"fieldPath\":\"createdAt\","
              "\"setToServerValue\":\"REQUEST_TIME\"}],"
            "\"currentDocument\":{\"exists\":false}"
            "},"
            "{\"update\":{"
              "\"name\":\"projects/%s/databases/(default)/documents/jdnextHomeworkPrivate/%s/entries/%s\","
              "\"fields\":{"
                "\"authorAuthUid\":{\"stringValue\":\"%s\"},"
                "\"authorTobiUid\":{\"nullValue\":null},"
                "\"authorTobiAccountUid\":%s"
              "}"
            "},"
            "\"updateTransforms\":[{\"fieldPath\":\"createdAt\","
              "\"setToServerValue\":\"REQUEST_TIME\"}],"
            "\"currentDocument\":{\"exists\":false}"
          "}"
          "]}",
          TOBI_PROJECT, hw_key, entry_id, safe_text, safe_author,
          TOBI_PROJECT, hw_key, entry_id, auth_uid, owner_account_value);
    }

    if (needed < 0 || (size_t)needed >= sizeof(body)) {
        snprintf(g_status, sizeof(g_status), "Hausaufgabe konnte nicht vorbereitet werden (zu gross).");
        return false;
    }

    HttpResponse response;
    bool ok = firestore_commit(TOBI_PROJECT, body, token, &response);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "Hausaufgabe abgelehnt (HTTP %ld).", response.status);
        if (response.data && strstr(response.data, "message")) {
            strncat(g_status, " Firestore-Regeln oder Anmeldung pruefen.",
                sizeof(g_status) - strlen(g_status) - 1);
        }
    } else {
        snprintf(g_status, sizeof(g_status), reward
            ? "Gespeichert: +10 Goldmuenzen."
            : "Hausaufgabe gespeichert und live geteilt.");
    }
    http_response_free(&response);
    return ok;
}

static bool call_function_with_token(const char *name, const char *data_json,
                                     const char *token, HttpResponse *out) {
    if (!token || !token[0]) {
        snprintf(g_status, sizeof(g_status), "Bitte zuerst eine Firebase-Sitzung herstellen.");
        return false;
    }
    char url[500], body[4000];
    snprintf(url, sizeof(url),
        "https://europe-west1-tobiservices.cloudfunctions.net/%s", name);
    snprintf(body, sizeof(body), "{\"data\":%s}", data_json ? data_json : "{}");
    return http_request(url, "POST", body, token, out);
}

static bool call_function(const char *name, const char *data_json, HttpResponse *out) {
    if (!g_account.signedIn || !g_account.idToken[0]) {
        snprintf(g_status, sizeof(g_status), "Bitte zuerst mit TobiServices anmelden.");
        return false;
    }
    return call_function_with_token(name, data_json, g_account.idToken, out);
}

static bool report_homework_entry(const char *hw_key, const char *entry_id) {
    const char *token = NULL;
    if (g_account.signedIn) {
        token = g_account.idToken;
    } else {
        if (!firebase_anonymous_session(TOBI_API_KEY, &g_guestTobi)) return false;
        token = g_guestTobi.idToken;
    }

    char safe_key[180], safe_id[260], payload[520];
    json_escape(hw_key, safe_key, sizeof(safe_key));
    json_escape(entry_id, safe_id, sizeof(safe_id));
    snprintf(payload, sizeof(payload),
        "{\"hwKey\":\"%s\",\"entryId\":\"%s\"}", safe_key, safe_id);

    HttpResponse response;
    bool ok = call_function_with_token(
        "reportJdnextHomeworkEntryNative", payload, token, &response);
    if (!ok) {
        snprintf(g_status, sizeof(g_status),
            "Meldung fehlgeschlagen (HTTP %ld).", response.status);
        if (response.data && strstr(response.data, "already-exists")) {
            snprintf(g_status, sizeof(g_status), "Du hast diesen Eintrag bereits gemeldet.");
        } else if (response.data && strstr(response.data, "permission-denied")) {
            snprintf(g_status, sizeof(g_status), "Eigene Hausaufgaben koennen nicht gemeldet werden.");
        }
    } else {
        snprintf(g_status, sizeof(g_status), "Meldung gespeichert. Der Report-Zaehler wurde aktualisiert.");
    }
    http_response_free(&response);
    return ok;
}

/* ----------------------------- Account data ---------------------------- */
static void refresh_profile(void) {
    if (!g_account.signedIn) return;
    char path[320];
    snprintf(path, sizeof(path), "users/%s", g_account.uid);
    HttpResponse r;
    if (!firestore_get(TOBI_PROJECT, path, g_account.idToken, &r)) {
        http_response_free(&r);
        snprintf(g_status, sizeof(g_status), "Profil konnte nicht geladen werden.");
        return;
    }
    JsonToken *t = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    int n = 0, root = json_parse(r.data, t, &n);
    int fields = root >= 0 ? json_object_get(r.data, t, n, root, "fields") : -1;
    g_account.goldCoins = firestore_field_int(r.data, t, n, fields, "goldCoins");
    g_account.disabled = firestore_field_bool(r.data, t, n, fields, "disabled");
    firestore_field_string_array(r.data, t, n, fields, "ownedThemes",
        g_account.ownedThemes, sizeof(g_account.ownedThemes));
    firestore_field_string(r.data, t, n, fields, "equippedTheme",
        g_account.equippedTheme, sizeof(g_account.equippedTheme));
    firestore_field_string(r.data, t, n, fields, "profileTitle",
        g_account.profileTitle, sizeof(g_account.profileTitle));
    firestore_field_string(r.data, t, n, fields, "profileIcon",
        g_account.profileIcon, sizeof(g_account.profileIcon));
    ui_set_theme(g_account.equippedTheme[0] ? g_account.equippedTheme : "Next");
    int tags_wrapper = json_object_get(r.data, t, n, fields, "tags");
    int tags_array = json_object_get(r.data, t, n, tags_wrapper, "arrayValue");
    int values = json_object_get(r.data, t, n, tags_array, "values");
    g_account.tags[0] = 0;
    g_account.isAdmin = false;
    if (values >= 0 && t[values].type == 'a') {
        for (int i = 0; i < 30; i++) {
            int entry = json_array_get(t, n, values, i);
            if (entry < 0) break;
            int string_value = json_object_get(r.data, t, n, entry, "stringValue");
            char tag[80];
            json_copy_string(r.data, t, string_value, tag, sizeof(tag));
            if (!tag[0]) continue;
            if (g_account.tags[0]) strncat(g_account.tags, ", ", sizeof(g_account.tags) - strlen(g_account.tags) - 1);
            strncat(g_account.tags, tag, sizeof(g_account.tags) - strlen(g_account.tags) - 1);
            if (strcasecmp(tag, "admin") == 0) g_account.isAdmin = true;
        }
    }
    if (g_account.disabled) g_account.isAdmin = false;
    free(t);
    http_response_free(&r);
}

static void logout_account(void) {
    memset(&g_account, 0, sizeof(g_account));
    snprintf(g_status, sizeof(g_status), "Du wurdest abgemeldet.");
}

static void load_maintenance(void) {
    HttpResponse r;
    if (!firestore_get(TOBI_PROJECT, "siteSettings/jdnext", NULL, &r)) {
        http_response_free(&r);
        return;
    }
    JsonToken *t = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    int n = 0, root = json_parse(r.data, t, &n);
    int fields = root >= 0 ? json_object_get(r.data, t, n, root, "fields") : -1;
    g_maintenance = firestore_field_bool(r.data, t, n, fields, "enabled");
    firestore_field_string(r.data, t, n, fields, "message",
                            g_maintenance_message, sizeof(g_maintenance_message));
    free(t);
    http_response_free(&r);
}

static bool is_week_a(void) {
    struct tm base = {0}, current = g_week_monday;
    base.tm_year = 2026 - 1900;
    base.tm_mon = 8;
    base.tm_mday = 21; /* Monday configured as the first A-week. */
    base.tm_hour = current.tm_hour = 12;
    time_t base_time = mktime(&base);
    time_t week_time = mktime(&current);
    struct tm b = *localtime(&base_time);
    struct tm w = *localtime(&week_time);
    /* Convert calendar dates to day counts, avoiding DST's 23/25-hour days. */
    int base_days = b.tm_yday + 365 * (b.tm_year - 100);
    int week_days = w.tm_yday + 365 * (w.tm_year - 100);
    int base_year = b.tm_year + 1900;
    int week_year = w.tm_year + 1900;
    int leap_years_base = (base_year - 1) / 4 - (base_year - 1) / 100 + (base_year - 1) / 400;
    int leap_years_week = (week_year - 1) / 4 - (week_year - 1) / 100 + (week_year - 1) / 400;
    int weeks = (week_days + leap_years_week - base_days - leap_years_base) / 7;
    return (weeks % 2) == 0;
}

static struct tm date_for_day(int day) {
    struct tm d = g_week_monday;
    d.tm_mday += day;
    mktime(&d);
    return d;
}

static void date_key_for_lesson(int day, int period, char *out, size_t cap) {
    struct tm d = date_for_day(day);
    snprintf(out, cap, "%04d-%02d-%02d_%d",
        d.tm_year + 1900, d.tm_mon + 1, d.tm_mday, period);
}

static const char *subject_for(int day, int period) {
    return is_week_a() ? SCHEDULE_A[day][period] : SCHEDULE_B[day][period];
}

static void format_date(struct tm d, char *out, size_t cap) {
    snprintf(out, cap, "%02d.%02d.%04d", d.tm_mday, d.tm_mon + 1, d.tm_year + 1900);
}

/* ---------------------------- Lesson data ------------------------------ */
static void add_entry(const char *id, const char *title, const char *author,
                        int report_count, bool classwork, bool legacy) {
    if (g_entry_count >= MAX_ENTRIES || !title[0]) return;
    LessonEntry *e = &g_entries[g_entry_count++];
    snprintf(e->id, sizeof(e->id), "%s", id ? id : "");
    snprintf(e->title, sizeof(e->title), "%s", title);
    snprintf(e->author, sizeof(e->author), "%s", author ? author : "");
    e->reportCount = report_count;
    e->classwork = classwork;
    e->legacy = legacy;
}

static void parse_lesson_documents(const char *json, bool classwork, bool legacy) {
    JsonToken *t = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    if (!t) return;
    int n = 0, root = json_parse(json, t, &n);
    if (root < 0) { free(t); return; }
    int docs = json_object_get(json, t, n, root, "documents");
    for (int i = 0; i < MAX_ENTRIES; i++) {
        int doc = json_array_get(t, n, docs, i);
        if (doc < 0) break;
        int fields = json_object_get(json, t, n, doc, "fields");
        char title[300] = "", author[100] = "", full_name[420] = "", id[128] = "";
        json_get_string(json, t, n, doc, "name", full_name, sizeof(full_name));
        const char *last_slash = strrchr(full_name, '/');
        if (last_slash) snprintf(id, sizeof(id), "%s", last_slash + 1);
        if (classwork) {
            firestore_field_string(json, t, n, fields, "title", title, sizeof(title));
        } else {
            firestore_field_string(json, t, n, fields, "text", title, sizeof(title));
            firestore_field_string(json, t, n, fields, "author", author, sizeof(author));
        }
        int reports = classwork ? 0 : firestore_field_int(json, t, n, fields, "reportCount");
        add_entry(id, title, author, reports, classwork, legacy);
    }
    free(t);
}

static void load_lesson_entries(void) {
    g_entry_count = 0;
    char key[64], path[256];
    date_key_for_lesson(g_day, g_period, key, sizeof(key));

    snprintf(path, sizeof(path), "jdnextHomework/%s/entries", key);
    HttpResponse r;
    if (firestore_list(TOBI_PROJECT, path, NULL, &r)) {
        parse_lesson_documents(r.data, false, false);
    }
    http_response_free(&r);

    snprintf(path, sizeof(path), "homework/%s/entries", key);
    if (firestore_list(OLD_PROJECT, path, NULL, &r)) {
        parse_lesson_documents(r.data, false, true);
    }
    http_response_free(&r);

    snprintf(path, sizeof(path), "classwork/%s/entries", key);
    if (firestore_list(OLD_PROJECT, path, NULL, &r)) {
        parse_lesson_documents(r.data, true, false);
    }
    http_response_free(&r);
}


static bool build_owned_themes_json(const char *extra_theme, char *out, size_t cap) {
    if (!cap) return false;
    size_t used = (size_t)snprintf(out, cap, "{\"arrayValue\":{\"values\":[");
    if (used >= cap) return false;

    char copy[sizeof(g_account.ownedThemes)];
    snprintf(copy, sizeof(copy), "%s", g_account.ownedThemes);
    char *save = NULL;
    char *theme = strtok_r(copy, "|", &save);
    int count = 0;
    while (theme) {
        if (theme[0]) {
            int written = snprintf(out + used, cap - used,
                "%s{\"stringValue\":\"%s\"}", count ? "," : "", theme);
            if (written < 0 || (size_t)written >= cap - used) return false;
            used += (size_t)written;
            count++;
        }
        theme = strtok_r(NULL, "|", &save);
    }

    if (extra_theme && extra_theme[0] && !account_owns_theme(extra_theme)) {
        int written = snprintf(out + used, cap - used,
            "%s{\"stringValue\":\"%s\"}", count ? "," : "", extra_theme);
        if (written < 0 || (size_t)written >= cap - used) return false;
        used += (size_t)written;
        count++;
    }

    int written = snprintf(out + used, cap - used, "]}}");
    return written >= 0 && (size_t)written < cap - used && count <= 30;
}

static bool equip_theme_native(const ThemeOption *theme) {
    if (!theme) return false;
    if (theme->local_only) {
        if (!g_account.signedIn || !g_account.isAdmin || g_account.disabled) {
            snprintf(g_status, sizeof(g_status), "Admin Matrix ist nur mit aktivem Admin-Tag verfuegbar.");
            return false;
        }
        ui_set_theme(theme->id);
        snprintf(g_account.equippedTheme, sizeof(g_account.equippedTheme), "%s", theme->id);
        snprintf(g_status, sizeof(g_status), "%s lokal ausgeruestet.", theme->name);
        return true;
    }

    if (!g_account.signedIn) {
        if (strcmp(theme->id, "Next") == 0) {
            ui_set_theme("Next");
            snprintf(g_status, sizeof(g_status), "Next ist das kostenlose Gast-Theme.");
            return true;
        }
        snprintf(g_status, sizeof(g_status), "Melde dich an, um dieses Theme auszuruesten.");
        return false;
    }
    if (g_account.disabled) {
        snprintf(g_status, sizeof(g_status), "Dieses Konto ist gesperrt.");
        return false;
    }
    if (strcmp(theme->id, "Next") != 0 && !account_owns_theme(theme->id)) {
        snprintf(g_status, sizeof(g_status), "Dieses Theme ist noch nicht freigeschaltet.");
        return false;
    }

    char body[1200];
    int needed = snprintf(body, sizeof(body),
        "{\"writes\":[{"
          "\"update\":{"
            "\"name\":\"projects/%s/databases/(default)/documents/users/%s\","
            "\"fields\":{\"equippedTheme\":{\"stringValue\":\"%s\"}}"
          "},"
          "\"updateMask\":{\"fieldPaths\":[\"equippedTheme\"]},"
          "\"currentDocument\":{\"exists\":true}"
        "}]}",
        TOBI_PROJECT, g_account.uid, theme->id);
    if (needed < 0 || (size_t)needed >= sizeof(body)) {
        snprintf(g_status, sizeof(g_status), "Theme-Aenderung konnte nicht vorbereitet werden.");
        return false;
    }

    HttpResponse response;
    bool ok = firestore_commit(TOBI_PROJECT, body, g_account.idToken, &response);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "Theme konnte nicht ausgeruestet werden (HTTP %ld).", response.status);
    } else {
        refresh_profile();
        ui_set_theme(theme->id);
        snprintf(g_status, sizeof(g_status), "%s ausgeruestet.", theme->name);
    }
    http_response_free(&response);
    return ok;
}

static bool purchase_theme_native(const ThemeOption *theme) {
    if (!theme || theme->price <= 0 || theme->event_only || theme->local_only) {
        snprintf(g_status, sizeof(g_status), "Dieses Theme ist nicht kaufbar.");
        return false;
    }
    if (!g_account.signedIn || g_account.disabled) {
        snprintf(g_status, sizeof(g_status), "Zum Kaufen brauchst du ein aktives TobiServices-Konto.");
        return false;
    }

    refresh_profile();
    if (account_owns_theme(theme->id)) {
        return equip_theme_native(theme);
    }
    if (g_account.goldCoins < theme->price) {
        snprintf(g_status, sizeof(g_status), "Du brauchst %d Goldmuenzen fuer %s.",
            theme->price, theme->name);
        return false;
    }

    char owned_json[3000], event_id[256], body[6500];
    if (!build_owned_themes_json(theme->id, owned_json, sizeof(owned_json))) {
        snprintf(g_status, sizeof(g_status), "Deine Theme-Liste ist voll oder ungueltig.");
        return false;
    }
    snprintf(event_id, sizeof(event_id), "theme-%s-%s", g_account.uid, theme->id);

    int next_coins = g_account.goldCoins - theme->price;
    int needed = snprintf(body, sizeof(body),
        "{\"writes\":["
          "{\"update\":{"
            "\"name\":\"projects/%s/databases/(default)/documents/users/%s\","
            "\"fields\":{"
              "\"goldCoins\":{\"integerValue\":\"%d\"},"
              "\"ownedThemes\":%s,"
              "\"lastJdnextCoinEventId\":{\"stringValue\":\"%s\"}"
            "}"
          "},"
          "\"updateMask\":{\"fieldPaths\":[\"goldCoins\",\"ownedThemes\",\"lastJdnextCoinEventId\"]},"
          "\"updateTransforms\":[{\"fieldPath\":\"lastProfileChangeAt\","
            "\"setToServerValue\":\"REQUEST_TIME\"}],"
          "\"currentDocument\":{\"exists\":true}"
          "},"
          "{\"update\":{"
            "\"name\":\"projects/%s/databases/(default)/documents/jdnextCoinEvents/%s\","
            "\"fields\":{"
              "\"uid\":{\"stringValue\":\"%s\"},"
              "\"amount\":{\"integerValue\":\"-%d\"},"
              "\"type\":{\"stringValue\":\"theme-purchase\"},"
              "\"themeId\":{\"stringValue\":\"%s\"}"
            "}"
          "},"
          "\"updateTransforms\":[{\"fieldPath\":\"createdAt\","
            "\"setToServerValue\":\"REQUEST_TIME\"}],"
          "\"currentDocument\":{\"exists\":false}"
        "}"
        "]}",
        TOBI_PROJECT, g_account.uid, next_coins, owned_json, event_id,
        TOBI_PROJECT, event_id, g_account.uid, theme->price, theme->id);

    if (needed < 0 || (size_t)needed >= sizeof(body)) {
        snprintf(g_status, sizeof(g_status), "Theme-Kauf konnte nicht vorbereitet werden.");
        return false;
    }

    HttpResponse response;
    bool ok = firestore_commit(TOBI_PROJECT, body, g_account.idToken, &response);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "Theme-Kauf abgelehnt (HTTP %ld).", response.status);
    } else {
        refresh_profile();
        snprintf(g_status, sizeof(g_status), "%s gekauft. Neues Guthaben: %d.",
            theme->name, g_account.goldCoins);
    }
    http_response_free(&response);
    return ok;
}

static bool save_profile_cosmetics_native(void) {
    if (!g_account.signedIn || g_account.disabled) {
        snprintf(g_status, sizeof(g_status), "Zum Bearbeiten brauchst du ein aktives Konto.");
        return false;
    }
    const char *title = PROFILE_TITLE_IDS[g_profile_title_selection];
    const char *icon = PROFILE_ICON_IDS[g_profile_icon_selection];
    char safe_title[100], safe_icon[100], body[1800];
    json_escape(title, safe_title, sizeof(safe_title));
    json_escape(icon, safe_icon, sizeof(safe_icon));

    int needed = snprintf(body, sizeof(body),
        "{\"writes\":[{"
          "\"update\":{"
            "\"name\":\"projects/%s/databases/(default)/documents/users/%s\","
            "\"fields\":{"
              "\"profileTitle\":{\"stringValue\":\"%s\"},"
              "\"profileIcon\":{\"stringValue\":\"%s\"}"
            "}"
          "},"
          "\"updateMask\":{\"fieldPaths\":[\"profileTitle\",\"profileIcon\"]},"
          "\"updateTransforms\":[{\"fieldPath\":\"lastProfileChangeAt\","
            "\"setToServerValue\":\"REQUEST_TIME\"}],"
          "\"currentDocument\":{\"exists\":true}"
        "}]}",
        TOBI_PROJECT, g_account.uid, safe_title, safe_icon);

    if (needed < 0 || (size_t)needed >= sizeof(body)) {
        snprintf(g_status, sizeof(g_status), "Profil-Aenderung konnte nicht vorbereitet werden.");
        return false;
    }

    HttpResponse response;
    bool ok = firestore_commit(TOBI_PROJECT, body, g_account.idToken, &response);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "Profil konnte nicht gespeichert werden (HTTP %ld).", response.status);
    } else {
        snprintf(g_status, sizeof(g_status), "Profil-Design gespeichert.");
        refresh_profile();
    }
    http_response_free(&response);
    return ok;
}


/* ------------------------------ Native UI ------------------------------ */
static void draw_sidebar(const char *active) {
    ui_rect(0, 0, 220, 720, UI_SURFACE);
    ui_rect(0, 0, 5, 720, UI_CYAN);
    ui_rect(20, 24, 54, 54, UI_SURFACE_2);
    ui_outline(20, 24, 54, 54, UI_CYAN);
    ui_text(32, 42, "UN", 3, UI_CYAN);
    ui_text(86, 28, "UNTIS", 2, UI_TEXT);
    ui_text(86, 54, "NEXT", 2, UI_PURPLE);
    ui_separator(18, 98, 184, UI_BORDER);

    const char *items[] = { "STUNDENPLAN", "KONTO", "ADMIN" };
    const int ys[] = { 126, 180, 234 };
    for (int i = 0; i < 3; i++) {
        bool selected = strcmp(active, items[i]) == 0;
        if (selected) {
            ui_rect(16, ys[i] - 9, 188, 42, UI_SURFACE_2);
            ui_rect(16, ys[i] - 9, 4, 42, UI_CYAN);
        }
        ui_text(34, ys[i], items[i], 1, selected ? UI_CYAN : UI_MUTED);
    }

    ui_card(16, 570, 188, 112, false);
    ui_text(30, 588, "TOBISERVICES", 1, UI_MUTED);
    if (g_account.signedIn) {
        ui_text(30, 612, g_account.displayName[0] ? g_account.displayName : g_account.email, 1, UI_TEXT);
        ui_text(30, 634, g_account.isAdmin ? "ADMIN" : "ANGEMELDET", 1,
                g_account.isAdmin ? UI_GREEN : UI_CYAN);
    } else {
        ui_text(30, 615, "GASTMODUS", 1, UI_TEXT);
        ui_text(30, 638, "X: KONTO", 1, UI_MUTED);
    }
}

static void draw_top_header(const char *title, const char *subtitle) {
    ui_text(252, 38, title, 3, UI_TEXT);
    ui_text(254, 86, subtitle, 1, UI_MUTED);
    ui_separator(252, 120, 996, UI_BORDER);
}

static void draw_status_bar(void) {
    ui_rect(236, 650, 1044, 70, UI_SURFACE);
    ui_separator(236, 650, 1044, UI_BORDER);
    ui_text(254, 677, g_status[0] ? g_status : "Bereit.", 1, UI_MUTED);
    ui_text(1010, 677, "B ZURUECK  + BEENDEN", 1, UI_CYAN);
}

static void draw_loading(const char *message) {
    ui_begin();
    draw_sidebar("STUNDENPLAN");
    draw_top_header("UNTIS NEXT", "NATIVE SWITCH-ANWENDUNG");
    ui_card(360, 250, 570, 170, true);
    ui_text(405, 294, "BITTE WARTEN", 2, UI_CYAN);
    ui_text(405, 342, message ? message : "Lade Daten ...", 1, UI_TEXT);
    ui_rect(405, 378, 470, 4, UI_BORDER);
    ui_rect(405, 378, 130, 4, UI_PURPLE);
    draw_status_bar();
    ui_end();
}

static void draw_timetable(void) {
    ui_begin();
    draw_sidebar("STUNDENPLAN");
    char weekline[120], monday_text[40], friday_text[40], pill[40];
    struct tm friday = date_for_day(4);
    format_date(g_week_monday, monday_text, sizeof(monday_text));
    format_date(friday, friday_text, sizeof(friday_text));
    snprintf(weekline, sizeof(weekline), "%s  -  %s", monday_text, friday_text);
    snprintf(pill, sizeof(pill), "%s-WOCHE", is_week_a() ? "A" : "B");
    draw_top_header("STUNDENPLAN", weekline);
    ui_badge(1050, 42, 150, pill, UI_SURFACE_2, UI_CYAN);

    const int x0 = 252, y0 = 174;
    const int time_w = 86, cell_w = 178, gap = 8, head_h = 38, row_h = 58, row_gap = 7;
    ui_rect(x0, y0, time_w - 8, head_h, UI_SURFACE_2);
    ui_text(x0 + 20, y0 + 14, "STD.", 1, UI_MUTED);
    for (int d = 0; d < 5; d++) {
        char header[48], datepart[24];
        struct tm date = date_for_day(d);
        format_date(date, datepart, sizeof(datepart));
        snprintf(header, sizeof(header), "%s  %s", DAY_SHORT[d], datepart);
        int x = x0 + time_w + d * (cell_w + gap);
        ui_card(x, y0, cell_w, head_h, false);
        ui_text_center(x + 6, y0 + 14, cell_w - 12, header, 1, UI_TEXT);
    }

    for (int p = 0; p < 6; p++) {
        int y = y0 + head_h + 10 + p * (row_h + row_gap);
        ui_card(x0, y, time_w - 8, row_h, false);
        ui_text(x0 + 15, y + 10, PERIOD_LABELS[p], 2, UI_YELLOW);
        ui_text(x0 + 10, y + 38, PERIOD_TIMES[p], 1, UI_MUTED);
        for (int d = 0; d < 5; d++) {
            const char *subject = subject_for(d, p);
            bool selected = d == g_day && p == g_period;
            int x = x0 + time_w + d * (cell_w + gap);
            ui_card(x, y, cell_w, row_h, selected);
            if (subject[0]) {
                ui_text(x + 13, y + 21, subject, 2, selected ? UI_CYAN : UI_TEXT);
            } else {
                ui_text(x + 13, y + 23, "FREI", 1, UI_MUTED);
            }
            if (selected) ui_badge(x + cell_w - 48, y + 7, 38, "A", UI_CYAN, UI_BG);
        }
    }

    int selected_x = x0 + time_w + g_day * (cell_w + gap);
    int selected_y = y0 + head_h + 10 + g_period * (row_h + row_gap);
    (void)selected_x;
    (void)selected_y;
    if (g_maintenance) {
        ui_badge(980, 602, 230, "WARTUNG AKTIV", UI_RED, UI_BG);
        ui_text(252, 614, g_maintenance_message[0] ? g_maintenance_message : "Untis Next ist momentan in Wartung.",
                1, UI_RED);
    } else {
        ui_text(252, 614, "STEUERKREUZ: STUNDE  |  L/R: WOCHE  |  A: DETAILS  |  X: KONTO",
                1, UI_MUTED);
    }
    draw_status_bar();
    ui_end();
}

static void draw_lesson(void) {
    ui_begin();
    draw_sidebar("STUNDENPLAN");
    char date[40], subtitle[150];
    format_date(date_for_day(g_day), date, sizeof(date));
    snprintf(subtitle, sizeof(subtitle), "%s  |  %s STUNDE  |  %s",
             DAY_NAMES[g_day], PERIOD_LABELS[g_period], date);
    draw_top_header(subject_for(g_day, g_period), subtitle);

    ui_card(252, 154, 625, 468, false);
    ui_text(278, 178, "HAUSAUFGABEN", 2, UI_CYAN);
    ui_badge(742, 172, 106, "A  NEU", UI_SURFACE_2, UI_CYAN);
    int hw_row = 0;
    for (int i = 0; i < g_entry_count; i++) {
        if (g_entries[i].classwork) continue;
        int y = 222 + hw_row * 67;
        bool selected = g_menu_selection == i;
        ui_card(270, y, 590, 57, selected);
        ui_text(286, y + 12, g_entries[i].title, 1, selected ? UI_CYAN : UI_TEXT);
        if (g_entries[i].author[0]) ui_text(286, y + 34, g_entries[i].author, 1, UI_MUTED);
        if (g_entries[i].legacy) ui_badge(738, y + 15, 100, "ALT", UI_SURFACE_2, UI_YELLOW);
        hw_row++;
        if (hw_row >= 5) break;
    }
    if (!hw_row) ui_text(286, 236, "NOCH KEINE HAUSAUFGABEN", 1, UI_MUTED);

    ui_card(895, 154, 353, 468, false);
    ui_text(921, 178, "KLASSENARBEITEN", 1, UI_YELLOW);
    ui_badge(1178, 172, 54, "X +", UI_SURFACE_2, UI_YELLOW);
    int ka_row = 0;
    for (int i = 0; i < g_entry_count; i++) {
        if (!g_entries[i].classwork) continue;
        int y = 222 + ka_row * 63;
        ui_card(914, y, 315, 54, false);
        ui_text(930, y + 20, g_entries[i].title, 1, UI_TEXT);
        ka_row++;
        if (ka_row >= 5) break;
    }
    if (!ka_row) ui_text(920, 236, "KEINE KLASSENARBEIT", 1, UI_MUTED);

    ui_text(252, 629, "A HAUSAUFGABE  |  X KLASSENARBEIT  |  R MELDEN  |  Y AKTUALISIEREN",
            1, UI_MUTED);
    draw_status_bar();
    ui_end();
}

static void draw_account(void) {
    ui_begin();
    draw_sidebar("KONTO");
    draw_top_header("TOBISERVICES-KONTO", "ANMELDUNG UND PROFIL");
    ui_card(252, 154, 470, 420, false);
    if (g_account.signedIn) {
        ui_badge(278, 180, 150, "ANGEMELDET", UI_GREEN, UI_BG);
        ui_text(278, 232, g_account.displayName[0] ? g_account.displayName : "TOBISERVICES",
                2, UI_TEXT);
        ui_text(278, 270, g_account.email, 1, UI_MUTED);
        ui_separator(278, 302, 410, UI_BORDER);
        ui_text(278, 330, "GOLDMUENZEN", 1, UI_MUTED);
        char coins[32];
        snprintf(coins, sizeof(coins), "%d", g_account.goldCoins);
        ui_text(278, 358, coins, 3, UI_YELLOW);
        ui_text(278, 426, "AUSGERUESTET", 1, UI_MUTED);
        ui_text(278, 452, g_account.equippedTheme[0] ? g_account.equippedTheme : "Next", 1, UI_CYAN);
        ui_text(278, 500, g_account.isAdmin ? "ADMIN-ZUGRIFF AKTIV" : "STANDARDKONTO",
                1, g_account.isAdmin ? UI_GREEN : UI_MUTED);
    } else {
        ui_badge(278, 180, 118, "GASTMODUS", UI_SURFACE_2, UI_CYAN);
        ui_text(278, 238, "NICHT ANGEMELDET", 2, UI_TEXT);
        ui_text(278, 280, "Oeffentliche Stundenplan-Daten", 1, UI_MUTED);
        ui_text(278, 306, "bleiben ohne Konto verfuegbar.", 1, UI_MUTED);
        ui_text(278, 370, "Themes kannst du ansehen.", 1, UI_MUTED);
        ui_text(278, 396, "Kaeufe und Profil bleiben gesperrt.", 1, UI_MUTED);
    }

    ui_card(750, 154, 498, 420, true);
    ui_text(780, 182, "KONTO-AKTIONEN", 1, UI_MUTED);
    const char *guest_items[] = {
        "THEMES ANSEHEN",
        "MIT TOBISERVICES ANMELDEN",
        "ZURUECK ZUM STUNDENPLAN"
    };
    const char *user_items[] = {
        "THEMES UND PROFIL",
        "ABMELDEN",
        "ZURUECK ZUM STUNDENPLAN"
    };
    const char *admin_items[] = {
        "THEMES UND PROFIL",
        "ADMIN CENTER",
        "ABMELDEN",
        "ZURUECK ZUM STUNDENPLAN"
    };
    int count = !g_account.signedIn ? 3 : (g_account.isAdmin ? 4 : 3);
    for (int i = 0; i < count; i++) {
        int y = 222 + i * 72;
        bool selected = g_menu_selection == i;
        ui_card(774, y, 450, 56, selected);
        const char *label = !g_account.signedIn ? guest_items[i]
            : (g_account.isAdmin ? admin_items[i] : user_items[i]);
        ui_text(796, y + 21, label, 1, selected ? UI_CYAN : UI_TEXT);
    }
    ui_text(252, 629, "STEUERKREUZ: AUSWAHL  |  A: BESTAETIGEN  |  B: ZURUECK",
            1, UI_MUTED);
    draw_status_bar();
    ui_end();
}

static void draw_themes(void) {
    ui_begin();
    draw_sidebar("KONTO");
    char coin_text[80];
    snprintf(coin_text, sizeof(coin_text), "%d GOLDMUENZEN", g_account.signedIn ? g_account.goldCoins : 0);
    draw_top_header("THEMES", coin_text);
    int count = visible_theme_count();
    for (int i = 0; i < count; i++) {
        const ThemeOption *theme = visible_theme_at(i);
        if (!theme) continue;
        int column = i / 7;
        int row = i % 7;
        int x = 252 + column * 500;
        int y = 145 + row * 68;
        bool selected = g_menu_selection == i;
        bool owned = strcmp(theme->id, "Next") == 0 || account_owns_theme(theme->id)
            || (theme->local_only && g_account.isAdmin);
        bool equipped = strcmp(g_account.equippedTheme[0] ? g_account.equippedTheme : "Next",
                               theme->id) == 0;
        ui_card(x, y, 478, 58, selected);
        ui_text(x + 15, y + 10, theme->name, 1, selected ? UI_CYAN : UI_TEXT);
        ui_text(x + 15, y + 34, theme->id, 1, UI_MUTED);
        if (equipped) ui_badge(x + 344, y + 16, 120, "AUSGERUESTET", UI_GREEN, UI_BG);
        else if (owned) ui_badge(x + 344, y + 16, 120, "BESITZT", UI_SURFACE_2, UI_CYAN);
        else if (theme->price > 0) {
            char price[32];
            snprintf(price, sizeof(price), "%d GOLD", theme->price);
            ui_badge(x + 344, y + 16, 120, price, UI_SURFACE_2, UI_YELLOW);
        } else ui_badge(x + 344, y + 16, 120, "FREIGESCHALTET", UI_SURFACE_2, UI_GREEN);
    }
    ui_text(252, 629, "A KAUFEN/AUSRUESTEN  |  X PROFIL-DESIGN  |  B ZURUECK",
            1, UI_MUTED);
    draw_status_bar();
    ui_end();
}

static void draw_profile(void) {
    ui_begin();
    draw_sidebar("KONTO");
    draw_top_header("PROFIL-DESIGN", "TITEL UND PROFIL-ICON");
    ui_card(252, 160, 996, 190, true);
    ui_text(280, 188, "PROFIL-TITEL", 1, UI_MUTED);
    ui_text(280, 230, PROFILE_TITLE_LABELS[g_profile_title_selection], 3, UI_CYAN);
    ui_text(280, 292, "STEUERKREUZ HOCH/RUNTER ZUM AUSWAEHLEN", 1, UI_MUTED);

    ui_card(252, 372, 996, 190, true);
    ui_text(280, 400, "PROFIL-ICON", 1, UI_MUTED);
    ui_text(280, 442, PROFILE_ICON_LABELS[g_profile_icon_selection], 3, UI_PURPLE);
    ui_text(280, 506, "STEUERKREUZ LINKS/RECHTS ZUM AUSWAEHLEN", 1, UI_MUTED);

    ui_text(252, 629, "A SPEICHERN  |  B ZURUECK", 1, UI_MUTED);
    draw_status_bar();
    ui_end();
}

static void draw_admin(void) {
    ui_begin();
    draw_sidebar("ADMIN");
    draw_top_header("TOBISERVICES ADMIN", "NATIVE ADMINISTRATION");
    ui_card(252, 154, 996, 118, false);
    ui_badge(278, 178, 184, "ADMIN BESTAETIGT", UI_GREEN, UI_BG);
    ui_text(278, 222, g_account.email, 1, UI_TEXT);
    ui_badge(1058, 178, 160, g_maintenance ? "WARTUNG AN" : "WARTUNG AUS",
             g_maintenance ? UI_RED : UI_SURFACE_2, g_maintenance ? UI_BG : UI_GREEN);

    const char *items[] = {
        "SWITCH NRO NEU KOMPILIEREN",
        "WARTUNGSMODUS UMSCHALTEN",
        "ACCOUNTLISTE ANZEIGEN",
        "ZURUECK ZUM KONTO"
    };
    const char *descriptions[] = {
        "Startet GitHub Actions und aktualisiert den Download.",
        "Schaltet den Wartungshinweis fuer Untis Next.",
        "Zeigt Konten und deren Berechtigungsstatus.",
        "Zurueck zu deinen Kontoinformationen."
    };
    for (int i = 0; i < 4; i++) {
        int y = 300 + i * 78;
        bool selected = g_menu_selection == i;
        ui_card(252, y, 996, 64, selected);
        ui_text(278, y + 12, items[i], 1, selected ? UI_CYAN : UI_TEXT);
        ui_text(278, y + 36, descriptions[i], 1, UI_MUTED);
        ui_text(1194, y + 22, ">", 2, selected ? UI_CYAN : UI_MUTED);
    }
    ui_text(252, 629, "NUR AKTIVE ADMINS  |  JEDE AENDERUNG WIRD SERVERSEITIG GEPRUEFT",
            1, UI_MUTED);
    draw_status_bar();
    ui_end();
}

static bool keyboard_input(const char *header, const char *guide, bool password,
                           char *out, size_t cap) {
    if (!cap) return false;
    SwkbdConfig kbd;
    Result rc = swkbdCreate(&kbd, 0);
    if (R_FAILED(rc)) {
        snprintf(g_status, sizeof(g_status), "Switch-Tastatur konnte nicht geoeffnet werden.");
        return false;
    }
    if (password) swkbdConfigMakePresetPassword(&kbd);
    else swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, header);
    swkbdConfigSetGuideText(&kbd, guide);
    rc = swkbdShow(&kbd, out, cap);
    swkbdClose(&kbd);
    return R_SUCCEEDED(rc) && out[0] != 0;
}

/* ---------------------------- Admin actions ---------------------------- */
static void request_nro_build(void) {
    HttpResponse r;
    bool ok = call_function("requestSwitchNroBuild", "{}", &r);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "NRO-Build abgelehnt (HTTP %ld).", r.status);
        if (r.data && strstr(r.data, "message")) strncat(g_status, " Firebase/GitHub-Fehler.", sizeof(g_status) - strlen(g_status) - 1);
        http_response_free(&r);
        return;
    }
    snprintf(g_status, sizeof(g_status), "NRO-Build gestartet. Den Status findest du im GitHub-Workflow.");
    http_response_free(&r);
}

static void toggle_maintenance(void) {
    char data[900], safe_message[700], message[500] = "";
    bool next = !g_maintenance;
    if (next) {
        if (!keyboard_input("Wartungsmeldung", "Optionale Nachricht fuer Besucher", false,
                            message, sizeof(message))) {
            snprintf(g_status, sizeof(g_status), "Wartungsmeldung nicht geaendert.");
            return;
        }
        json_escape(message, safe_message, sizeof(safe_message));
    } else {
        json_escape(g_maintenance_message, safe_message, sizeof(safe_message));
    }
    snprintf(data, sizeof(data), "{\"enabled\":%s,\"message\":\"%s\"}",
        next ? "true" : "false", next ? safe_message : "");
    HttpResponse r;
    bool ok = call_function("setMaintenanceMode", data, &r);
    if (ok) {
        g_maintenance = next;
        snprintf(g_maintenance_message, sizeof(g_maintenance_message), "%s", next ? message : "");
        snprintf(g_status, sizeof(g_status), "Wartungsmodus %s.", next ? "aktiviert" : "deaktiviert");
    } else {
        snprintf(g_status, sizeof(g_status), "Wartungsmodus konnte nicht geaendert werden (HTTP %ld).", r.status);
    }
    http_response_free(&r);
}

static bool parse_plain_bool(const char *json, JsonToken *tokens, int count,
                             int object, const char *field) {
    int value = json_object_get(json, tokens, count, object, field);
    return value >= 0 && tokens[value].type == 'p'
        && strncmp(json + tokens[value].start, "true", 4) == 0;
}

static bool load_admin_user_list(void) {
    HttpResponse r;
    if (!call_function("listTobiUsers", "{}", &r)) {
        snprintf(g_status, sizeof(g_status), "Accountliste nicht verfuegbar (HTTP %ld).", r.status);
        http_response_free(&r);
        return false;
    }

    JsonToken *t = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    if (!t) {
        http_response_free(&r);
        snprintf(g_status, sizeof(g_status), "Nicht genug Speicher fuer die Accountliste.");
        return false;
    }
    int n = 0, root = json_parse(r.data, t, &n);
    int result = root >= 0 ? json_object_get(r.data, t, n, root, "result") : -1;
    if (result < 0 && root >= 0) result = json_object_get(r.data, t, n, root, "data");
    int users = result >= 0 ? json_object_get(r.data, t, n, result, "users") : -1;

    g_admin_user_count = 0;
    for (int i = 0; i < 24; i++) {
        int item = json_array_get(t, n, users, i);
        if (item < 0) break;
        AdminUser *user = &g_admin_users[g_admin_user_count++];
        memset(user, 0, sizeof(*user));
        json_get_string(r.data, t, n, item, "uid", user->uid, sizeof(user->uid));
        json_get_string(r.data, t, n, item, "displayName", user->displayName, sizeof(user->displayName));
        json_get_string(r.data, t, n, item, "email", user->email, sizeof(user->email));
        user->goldCoins = 0;
        int coins = json_object_get(r.data, t, n, item, "goldCoins");
        if (coins >= 0 && t[coins].type == 'p') {
            char number[32];
            size_t len = (size_t)(t[coins].end - t[coins].start);
            if (len > 0 && len < sizeof(number)) {
                memcpy(number, r.data + t[coins].start, len);
                number[len] = 0;
                user->goldCoins = atoi(number);
            }
        }
        user->disabled = parse_plain_bool(r.data, t, n, item, "disabled");
        int tags = json_object_get(r.data, t, n, item, "tags");
        for (int j = 0; j < 20; j++) {
            int tag_item = json_array_get(t, n, tags, j);
            if (tag_item < 0) break;
            char tag[40];
            json_copy_string(r.data, t, tag_item, tag, sizeof(tag));
            if (!tag[0]) continue;
            if (user->tags[0]) strncat(user->tags, ", ",
                sizeof(user->tags) - strlen(user->tags) - 1);
            strncat(user->tags, tag, sizeof(user->tags) - strlen(user->tags) - 1);
        }
    }

    free(t);
    http_response_free(&r);
    return true;
}

static void draw_admin_users_screen(int selected) {
    ui_begin();
    draw_sidebar("ADMIN");
    draw_top_header("ACCOUNT-MANAGER", "A SPERRE UMSCHALTEN  |  X TAGS BEARBEITEN  |  Y AKTUALISIEREN");
    for (int i = 0; i < g_admin_user_count; i++) {
        int column = i / 12;
        int row = i % 12;
        int x = 252 + column * 500;
        int y = 145 + row * 39;
        bool active = i == selected;
        ui_card(x, y, 478, 33, active);
        ui_text(x + 10, y + 7, g_admin_users[i].displayName[0]
            ? g_admin_users[i].displayName : "(OHNE NAMEN)", 1, active ? UI_CYAN : UI_TEXT);
        ui_text(x + 206, y + 7, g_admin_users[i].email, 1, UI_MUTED);
        ui_badge(x + 388, y + 4, 80,
            g_admin_users[i].disabled ? "GESPERRT" : "AKTIV",
            g_admin_users[i].disabled ? UI_RED : UI_SURFACE_2,
            g_admin_users[i].disabled ? UI_BG : UI_GREEN);
    }
    if (!g_admin_user_count) ui_text(278, 170, "KEINE ACCOUNTS GEFUNDEN", 1, UI_MUTED);
    if (selected >= 0 && selected < g_admin_user_count) {
        ui_card(252, 630, 996, 35, false);
        ui_text(270, 643, g_admin_users[selected].tags[0]
            ? g_admin_users[selected].tags : "KEINE TAGS", 1, UI_MUTED);
    }
    draw_status_bar();
    ui_end();
}

static bool load_allowed_tags(char tags[][32], int *tag_count) {
    *tag_count = 0;
    HttpResponse r;
    if (!firestore_get(TOBI_PROJECT, "siteSettings/accessControl", g_account.idToken, &r)) {
        http_response_free(&r);
        snprintf(tags[(*tag_count)++], 32, "admin");
        snprintf(tags[(*tag_count)++], 32, "dance-beta");
        return false;
    }
    JsonToken *t = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    if (!t) {
        http_response_free(&r);
        return false;
    }
    int n = 0, root = json_parse(r.data, t, &n);
    int fields = root >= 0 ? json_object_get(r.data, t, n, root, "fields") : -1;
    int wrapper = json_object_get(r.data, t, n, fields, "tags");
    int array = json_object_get(r.data, t, n, wrapper, "arrayValue");
    int values = json_object_get(r.data, t, n, array, "values");
    if (values >= 0 && t[values].type == 'a') {
        for (int i = 0; i < 100 && *tag_count < 100; i++) {
            int item = json_array_get(t, n, values, i);
            if (item < 0) break;
            char tag[32];
            json_copy_string(r.data, t, item, tag, sizeof(tag));
            if (tag[0]) snprintf(tags[(*tag_count)++], 32, "%s", tag);
        }
    }
    free(t);
    http_response_free(&r);
    if (*tag_count == 0) {
        snprintf(tags[(*tag_count)++], 32, "admin");
        snprintf(tags[(*tag_count)++], 32, "dance-beta");
    }
    return true;
}

static bool edit_admin_user_tags(AdminUser *user) {
    char raw[500] = "";
    if (!keyboard_input("TobiServices-Tags", "Tags durch Kommas trennen", false,
                        raw, sizeof(raw))) {
        snprintf(g_status, sizeof(g_status), "Tag-Bearbeitung abgebrochen.");
        return false;
    }

    char allowed[100][32];
    int allowed_count = 0;
    load_allowed_tags(allowed, &allowed_count);

    char tags_json[2200] = "[";
    char parsed[500];
    snprintf(parsed, sizeof(parsed), "%s", raw);
    char *save = NULL;
    char *part = strtok_r(parsed, ",", &save);
    int count = 0;
    while (part && count < 20) {
        while (*part && isspace((unsigned char)*part)) part++;
        size_t len = strlen(part);
        while (len && isspace((unsigned char)part[len - 1])) part[--len] = 0;
        for (size_t i = 0; i < len; i++) part[i] = (char)tolower((unsigned char)part[i]);

        if (len == 0) {
            part = strtok_r(NULL, ",", &save);
            continue;
        }
        if (len > 24) {
            snprintf(g_status, sizeof(g_status), "Ein Tag ist laenger als 24 Zeichen.");
            return false;
        }
        for (size_t i = 0; i < len; i++) {
            unsigned char ch = (unsigned char)part[i];
            if (!(isalnum(ch) || ch == '_' || ch == '-' || ch >= 0x80)) {
                snprintf(g_status, sizeof(g_status), "Ungueltiges Zeichen in einem Tag.");
                return false;
            }
        }

        bool is_allowed = false;
        for (int j = 0; j < allowed_count; j++) {
            if (strcmp(allowed[j], part) == 0) { is_allowed = true; break; }
        }
        if (!is_allowed) {
            snprintf(g_status, sizeof(g_status), "Tag '%s' ist nicht freigegeben.", part);
            return false;
        }
        char safe_tag[80];
        json_escape(part, safe_tag, sizeof(safe_tag));
        size_t used = strlen(tags_json);
        int written = snprintf(tags_json + used, sizeof(tags_json) - used,
            "%s\"%s\"", count ? "," : "", safe_tag);
        if (written < 0 || (size_t)written >= sizeof(tags_json) - used) return false;
        count++;
        part = strtok_r(NULL, ",", &save);
    }
    strncat(tags_json, "]", sizeof(tags_json) - strlen(tags_json) - 1);

    if (strcmp(user->uid, g_account.uid) == 0 && !strstr(tags_json, "\"admin\"")) {
        snprintf(g_status, sizeof(g_status), "Du kannst deinen eigenen Admin-Tag nicht entfernen.");
        return false;
    }

    char safe_uid[400], payload[2800];
    json_escape(user->uid, safe_uid, sizeof(safe_uid));
    snprintf(payload, sizeof(payload), "{\"uid\":\"%s\",\"tags\":%s}", safe_uid, tags_json);
    HttpResponse response;
    bool ok = call_function("setTobiTags", payload, &response);
    if (!ok) snprintf(g_status, sizeof(g_status), "Tags konnten nicht gespeichert werden (HTTP %ld).", response.status);
    else snprintf(g_status, sizeof(g_status), "Tags fuer %s aktualisiert.", user->displayName);
    http_response_free(&response);
    return ok;
}

static bool toggle_admin_user_disabled(AdminUser *user) {
    bool next = !user->disabled;
    char safe_uid[400], payload[700];
    json_escape(user->uid, safe_uid, sizeof(safe_uid));
    snprintf(payload, sizeof(payload),
        "{\"uid\":\"%s\",\"disabled\":%s}", safe_uid, next ? "true" : "false");
    HttpResponse response;
    bool ok = call_function("setTobiDisabled", payload, &response);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "Kontostatus konnte nicht geaendert werden (HTTP %ld).",
            response.status);
    } else {
        user->disabled = next;
        snprintf(g_status, sizeof(g_status), "Konto %s.", next ? "gesperrt" : "entsperrt");
    }
    http_response_free(&response);
    return ok;
}

static void show_admin_users(void) {
    if (!load_admin_user_list()) return;
    int selected = 0;
    while (appletMainLoop()) {
        draw_admin_users_screen(selected);
        PadState waitpad;
        padInitializeDefault(&waitpad);
        bool redraw = false;
        while (appletMainLoop()) {
            padUpdate(&waitpad);
            u64 pressed = padGetButtonsDown(&waitpad);
            if (pressed & HidNpadButton_B) goto done;
            if ((pressed & HidNpadButton_Up) && g_admin_user_count > 0) {
                selected = (selected + g_admin_user_count - 1) % g_admin_user_count;
                redraw = true;
                break;
            }
            if ((pressed & HidNpadButton_Down) && g_admin_user_count > 0) {
                selected = (selected + 1) % g_admin_user_count;
                redraw = true;
                break;
            }
            if (pressed & HidNpadButton_Y) {
                load_admin_user_list();
                if (g_admin_user_count > 0) selected %= g_admin_user_count;
                redraw = true;
                break;
            }
            if (g_admin_user_count > 0 && (pressed & HidNpadButton_A)) {
                toggle_admin_user_disabled(&g_admin_users[selected]);
                redraw = true;
                break;
            }
            if (g_admin_user_count > 0 && (pressed & HidNpadButton_X)) {
                if (edit_admin_user_tags(&g_admin_users[selected])) {
                    load_admin_user_list();
                    if (g_admin_user_count > 0) selected %= g_admin_user_count;
                }
                redraw = true;
                break;
            }
            if (pressed & HidNpadButton_Plus) goto done;
            svcSleepThread(16000000ULL);
        }
        if (redraw) continue;
    }
done:
    snprintf(g_status, sizeof(g_status), "Accountverwaltung geschlossen.");
}

/* ------------------------------- Actions ------------------------------ */
static void shift_week(int direction) {
    g_week_monday.tm_mday += direction * 7;
    mktime(&g_week_monday);
}

static void do_login(void) {
    char email[160] = "", password[200] = "";
    if (!keyboard_input("TobiServices-Anmeldung", "E-Mail-Adresse", false, email, sizeof(email))) return;
    if (!keyboard_input("TobiServices-Anmeldung", "Passwort", true, password, sizeof(password))) return;
    snprintf(g_status, sizeof(g_status), "Melde bei Firebase an ...");
    draw_loading("Melde dich bei TobiServices an ...");
    if (!firebase_sign_in(email, password)) {
        memset(password, 0, sizeof(password));
        return;
    }
    memset(password, 0, sizeof(password));
    refresh_profile();
    if (g_account.disabled) {
        logout_account();
        snprintf(g_status, sizeof(g_status), "Dieses TobiServices-Konto ist gesperrt.");
        return;
    }
    snprintf(g_status, sizeof(g_status), "Erfolgreich angemeldet.");
}

static void initialize_week(void) {
    time_t now = time(NULL);
    struct tm d = *localtime(&now);
    d.tm_hour = 12; d.tm_min = 0; d.tm_sec = 0;
    d.tm_mday -= (d.tm_wday + 6) % 7;
    g_week_monday = *localtime(&(time_t){mktime(&d)});
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    initialize_week();
    if (!ui_init()) {
        snprintf(g_status, sizeof(g_status), "Native libnx-Oberflaeche konnte nicht gestartet werden.");
    }
    Result socket_rc = socketInitializeDefault();
    g_socket_initialized = R_SUCCEEDED(socket_rc);
    if (!g_socket_initialized) {
        snprintf(g_status, sizeof(g_status), "Switch-Netzwerkdienst nicht verfuegbar (0x%08X).", (unsigned)socket_rc);
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* switch-curl uses libnx's native TLS service and system trust store. */
    if (g_socket_initialized) {
        load_maintenance();
        snprintf(g_status, sizeof(g_status), "Native Switch-App bereit. Keine Website wird geoeffnet.");
    }
    draw_timetable();

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);

        if (down & HidNpadButton_Plus) break;

        if (g_screen == SCREEN_TIMETABLE) {
            if (down & HidNpadButton_Left) { g_day = (g_day + 4) % 5; }
            if (down & HidNpadButton_Right) { g_day = (g_day + 1) % 5; }
            if (down & HidNpadButton_Up) { g_period = (g_period + 5) % 6; }
            if (down & HidNpadButton_Down) { g_period = (g_period + 1) % 6; }
            if (down & HidNpadButton_L) shift_week(-1);
            if (down & HidNpadButton_R) shift_week(1);
            if (down & HidNpadButton_Y) {
                load_maintenance();
                snprintf(g_status, sizeof(g_status), "Wartungsstatus aktualisiert.");
            }
            if (down & HidNpadButton_A) {
                if (subject_for(g_day, g_period)[0]) {
                    snprintf(g_status, sizeof(g_status), "Lade Hausaufgaben und Klassenarbeiten ...");
                    draw_loading("Lade Hausaufgaben und Klassenarbeiten ...");
                    load_lesson_entries();
                    g_menu_selection = 0;
                    g_screen = SCREEN_LESSON;
                } else {
                    snprintf(g_status, sizeof(g_status), "Diese Stunde ist als Freistunde eingetragen.");
                }
            }
            if (down & HidNpadButton_X) {
                g_screen = SCREEN_ACCOUNT;
                g_menu_selection = 0;
            }
            draw_timetable();
        } else if (g_screen == SCREEN_LESSON) {
            if (down & HidNpadButton_B) {
                g_screen = SCREEN_TIMETABLE;
            }
            if (down & HidNpadButton_Up) {
                if (g_entry_count > 0) {
                    g_menu_selection = (g_menu_selection + g_entry_count - 1) % g_entry_count;
                }
            }
            if (down & HidNpadButton_Down) {
                if (g_entry_count > 0) {
                    g_menu_selection = (g_menu_selection + 1) % g_entry_count;
                }
            }
            if (down & HidNpadButton_Y) {
                draw_loading("Aktualisiere Hausaufgaben und Klassenarbeiten ...");
                load_lesson_entries();
                if (g_entry_count > 0) g_menu_selection %= g_entry_count;
                snprintf(g_status, sizeof(g_status), "Eintraege aktualisiert.");
            }
            if (down & HidNpadButton_A) {
                char text[1001] = "";
                if (keyboard_input("Neue Hausaufgabe", "Aufgabentext (3 bis 1000 Zeichen)",
                                   false, text, sizeof(text))) {
                    const char *author = g_account.signedIn && g_account.displayName[0]
                        ? g_account.displayName : "Switch";
                    char key[64];
                    date_key_for_lesson(g_day, g_period, key, sizeof(key));
                    draw_loading("Speichere Hausaufgabe ...");
                    if (save_homework_entry(key, text, author)) {
                        load_lesson_entries();
                        g_menu_selection = 0;
                    }
                } else {
                    snprintf(g_status, sizeof(g_status), "Erstellen abgebrochen.");
                }
            }
            if (down & HidNpadButton_X) {
                char title[121] = "";
                if (keyboard_input("Neue Klassenarbeit", "Bezeichnung (2 bis 120 Zeichen)",
                                   false, title, sizeof(title))) {
                    char key[64];
                    date_key_for_lesson(g_day, g_period, key, sizeof(key));
                    draw_loading("Speichere Klassenarbeit ...");
                    if (save_classwork_entry(key, title)) {
                        load_lesson_entries();
                        g_menu_selection = 0;
                    }
                } else {
                    snprintf(g_status, sizeof(g_status), "Erstellen abgebrochen.");
                }
            }
            if (down & HidNpadButton_R) {
                if (g_entry_count > 0 && g_menu_selection >= 0 &&
                    g_menu_selection < g_entry_count &&
                    !g_entries[g_menu_selection].classwork &&
                    !g_entries[g_menu_selection].legacy) {
                    char key[64];
                    date_key_for_lesson(g_day, g_period, key, sizeof(key));
                    draw_loading("Sende Meldung ...");
                    if (report_homework_entry(key, g_entries[g_menu_selection].id)) {
                        load_lesson_entries();
                        if (g_entry_count > 0) g_menu_selection %= g_entry_count;
                        else g_menu_selection = 0;
                    }
                } else {
                    snprintf(g_status, sizeof(g_status), "Auswahl ist keine meldbare neue Hausaufgabe.");
                }
            }
            draw_lesson();
        } else if (g_screen == SCREEN_ACCOUNT) {
            int max_items = !g_account.signedIn ? 3 : (g_account.isAdmin ? 4 : 3);
            if (down & HidNpadButton_Up) g_menu_selection = (g_menu_selection + max_items - 1) % max_items;
            if (down & HidNpadButton_Down) g_menu_selection = (g_menu_selection + 1) % max_items;
            if (down & HidNpadButton_B) g_screen = SCREEN_TIMETABLE;
            if (down & HidNpadButton_A) {
                if (!g_account.signedIn) {
                    if (g_menu_selection == 0) {
                        g_screen = SCREEN_THEMES;
                        g_menu_selection = 0;
                    } else if (g_menu_selection == 1) {
                        do_login();
                    } else {
                        g_screen = SCREEN_TIMETABLE;
                    }
                } else if (g_menu_selection == 0) {
                    g_screen = SCREEN_THEMES;
                    g_menu_selection = 0;
                } else if (g_account.isAdmin && g_menu_selection == 1) {
                    load_maintenance();
                    g_screen = SCREEN_ADMIN;
                    g_menu_selection = 0;
                } else if ((g_account.isAdmin && g_menu_selection == 2)
                           || (!g_account.isAdmin && g_menu_selection == 1)) {
                    logout_account();
                    ui_set_theme("Next");
                    g_menu_selection = 0;
                } else {
                    g_screen = SCREEN_TIMETABLE;
                }
            }
            draw_account();
        } else if (g_screen == SCREEN_THEMES) {
            int count = visible_theme_count();
            if (down & HidNpadButton_Up && count > 0)
                g_menu_selection = (g_menu_selection + count - 1) % count;
            if (down & HidNpadButton_Down && count > 0)
                g_menu_selection = (g_menu_selection + 1) % count;
            if (down & HidNpadButton_B) g_screen = SCREEN_ACCOUNT;
            if (down & HidNpadButton_X) {
                if (g_account.signedIn) {
                    g_profile_title_selection = profile_title_index();
                    g_profile_icon_selection = profile_icon_index();
                    g_screen = SCREEN_PROFILE;
                } else {
                    snprintf(g_status, sizeof(g_status), "Melde dich an, um dein Profil anzupassen.");
                }
            }
            if (down & HidNpadButton_A && count > 0) {
                const ThemeOption *theme = visible_theme_at(g_menu_selection);
                if (theme) {
                    if (theme->price > 0 && !account_owns_theme(theme->id)) {
                        draw_loading("Pruefe Guthaben und kaufe Theme ...");
                        purchase_theme_native(theme);
                    } else {
                        equip_theme_native(theme);
                    }
                }
            }
            draw_themes();
        } else if (g_screen == SCREEN_PROFILE) {
            if (down & HidNpadButton_Up)
                g_profile_title_selection = (g_profile_title_selection + 6) % 7;
            if (down & HidNpadButton_Down)
                g_profile_title_selection = (g_profile_title_selection + 1) % 7;
            if (down & HidNpadButton_Left)
                g_profile_icon_selection = (g_profile_icon_selection + 13) % 14;
            if (down & HidNpadButton_Right)
                g_profile_icon_selection = (g_profile_icon_selection + 1) % 14;
            if (down & HidNpadButton_B) g_screen = SCREEN_THEMES;
            if (down & HidNpadButton_A) {
                draw_loading("Speichere Profil-Design ...");
                save_profile_cosmetics_native();
            }
            draw_profile();
        } else if (g_screen == SCREEN_ADMIN) {
            if (down & HidNpadButton_Up) g_menu_selection = (g_menu_selection + 3) % 4;
            if (down & HidNpadButton_Down) g_menu_selection = (g_menu_selection + 1) % 4;
            if (down & HidNpadButton_B) g_screen = SCREEN_ACCOUNT;
            if (down & HidNpadButton_A) {
                if (!g_account.isAdmin || g_account.disabled) {
                    g_screen = SCREEN_ACCOUNT;
                    snprintf(g_status, sizeof(g_status), "Admin-Sitzung ist nicht mehr gueltig.");
                } else if (g_menu_selection == 0) request_nro_build();
                else if (g_menu_selection == 1) toggle_maintenance();
                else if (g_menu_selection == 2) show_admin_users();
                else g_screen = SCREEN_ACCOUNT;
            }
            draw_admin();
        }

        svcSleepThread(16000000ULL);
    }

    curl_global_cleanup();
    ui_shutdown();
    if (g_socket_initialized) socketExit();
    return 0;
}
