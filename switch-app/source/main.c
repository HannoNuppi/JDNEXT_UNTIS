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
    int goldCoins;
    bool disabled;
    bool signedIn;
    bool isAdmin;
} Account;

typedef struct {
    char title[280];
    char author[96];
    bool classwork;
    bool legacy;
} LessonEntry;

typedef enum {
    SCREEN_TIMETABLE = 0,
    SCREEN_LESSON,
    SCREEN_ACCOUNT,
    SCREEN_ADMIN
} Screen;

static Account g_account;
static bool g_socket_initialized = false;
static bool g_maintenance = false;
static char g_maintenance_message[500];
static char g_status[220] = "Bereit.";
static struct tm g_week_monday;
static int g_day = 0;
static int g_period = 0;
static int g_menu_selection = 0;
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

static bool call_function(const char *name, const char *data_json, HttpResponse *out) {
    if (!g_account.signedIn || !g_account.idToken[0]) {
        snprintf(g_status, sizeof(g_status), "Bitte zuerst mit TobiServices anmelden.");
        return false;
    }
    char url[500], body[2200];
    snprintf(url, sizeof(url),
        "https://europe-west1-tobiservices.cloudfunctions.net/%s", name);
    snprintf(body, sizeof(body), "{\"data\":%s}", data_json ? data_json : "{}");
    return http_request(url, "POST", body, g_account.idToken, out);
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
static void add_entry(const char *title, const char *author, bool classwork, bool legacy) {
    if (g_entry_count >= MAX_ENTRIES || !title[0]) return;
    LessonEntry *e = &g_entries[g_entry_count++];
    snprintf(e->title, sizeof(e->title), "%s", title);
    snprintf(e->author, sizeof(e->author), "%s", author ? author : "");
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
        char title[300] = "", author[100] = "";
        if (classwork) {
            firestore_field_string(json, t, n, fields, "title", title, sizeof(title));
        } else {
            firestore_field_string(json, t, n, fields, "text", title, sizeof(title));
            firestore_field_string(json, t, n, fields, "author", author, sizeof(author));
        }
        add_entry(title, author, classwork, legacy);
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
        ui_card(270, y, 590, 57, selected, false);
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
        ui_text(278, 426, "TAGS", 1, UI_MUTED);
        ui_text(278, 452, g_account.tags[0] ? g_account.tags : "KEINE", 1, UI_TEXT);
        ui_text(278, 500, g_account.isAdmin ? "ADMIN-ZUGRIFF AKTIV" : "STANDARDKONTO",
                1, g_account.isAdmin ? UI_GREEN : UI_MUTED);
    } else {
        ui_badge(278, 180, 118, "GASTMODUS", UI_SURFACE_2, UI_CYAN);
        ui_text(278, 238, "NICHT ANGEMELDET", 2, UI_TEXT);
        ui_text(278, 280, "Oeffentliche Stundenplan-Daten", 1, UI_MUTED);
        ui_text(278, 306, "bleiben ohne Konto verfuegbar.", 1, UI_MUTED);
    }

    ui_card(750, 154, 498, 420, true);
    ui_text(780, 182, "KONTO-AKTIONEN", 1, UI_MUTED);
    const char *items_guest[] = { "MIT TOBISERVICES ANMELDEN", "ZURUECK ZUM STUNDENPLAN" };
    const char *items_user[] = { "ADMIN CENTER", "ABMELDEN", "ZURUECK ZUM STUNDENPLAN" };
    int count = g_account.signedIn ? 3 : 2;
    for (int i = 0; i < count; i++) {
        int y = 230 + i * 76;
        bool selected = g_menu_selection == i;
        ui_card(774, y, 450, 58, selected);
        ui_text(796, y + 22, g_account.signedIn ? items_user[i] : items_guest[i],
                1, selected ? UI_CYAN : UI_TEXT);
    }
    ui_text(252, 629, "STEUERKREUZ: AUSWAHL  |  A: BESTAETIGEN  |  B: ZURUECK",
            1, UI_MUTED);
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

static void show_admin_users(void) {
    HttpResponse r;
    bool ok = call_function("listTobiUsers", "{}", &r);
    if (!ok) {
        snprintf(g_status, sizeof(g_status), "Accountliste nicht verfuegbar (HTTP %ld).", r.status);
        http_response_free(&r);
        return;
    }
    clear_screen();
    print_header("ACCOUNT-MONITOR");
    JsonToken *t = (JsonToken *)calloc(MAX_JSON_TOKENS, sizeof(JsonToken));
    int n = 0, root = json_parse(r.data, t, &n);
    int data = root >= 0 ? json_object_get(r.data, t, n, root, "result") : -1;
    if (data < 0 && root >= 0) data = json_object_get(r.data, t, n, root, "data");
    int users = data >= 0 ? json_object_get(r.data, t, n, data, "users") : -1;
    int shown = 0;
    for (int i = 0; i < 24; i++) {
        int item = json_array_get(t, n, users, i);
        if (item < 0) break;
        char name[100] = "", email[140] = "", tags[140] = "";
        json_get_string(r.data, t, n, item, "displayName", name, sizeof(name));
        json_get_string(r.data, t, n, item, "email", email, sizeof(email));
        json_get_string(r.data, t, n, item, "uid", tags, sizeof(tags));
        printf("%2d. %s\n    %s\n    UID: %.60s\n\n",
            i + 1, name[0] ? name : "(ohne Namen)", email, tags);
        shown++;
    }
    if (!shown) printf("Keine Accounts oder Antwortformat nicht erkannt.\n");
    printf("\nB = Zurueck\n");
    consoleUpdate(NULL);
    PadState waitpad;
    padInitializeDefault(&waitpad);
    while (appletMainLoop()) {
        padUpdate(&waitpad);
        u64 pressed = padGetButtonsDown(&waitpad);
        if (pressed & HidNpadButton_B) break;
        if (pressed & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    free(t);
    http_response_free(&r);
    snprintf(g_status, sizeof(g_status), "%d Account-Eintraege angezeigt.", shown);
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
    consoleUpdate(NULL);
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

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    initialize_week();
    Result socket_rc = socketInitializeDefault();
    g_socket_initialized = R_SUCCEEDED(socket_rc);
    if (!g_socket_initialized) {
        snprintf(g_status, sizeof(g_status), "Switch-Netzwerkdienst nicht verfuegbar (0x%08X).", (unsigned)socket_rc);
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    consoleUpdate(NULL);

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
                    clear_screen(); print_header("LADEN"); printf("%s\n", g_status); consoleUpdate(NULL);
                    load_lesson_entries();
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
            if (down & HidNpadButton_B) g_screen = SCREEN_TIMETABLE;
            if (down & HidNpadButton_Y) {
                load_lesson_entries();
                snprintf(g_status, sizeof(g_status), "Eintraege aktualisiert.");
            }
            draw_lesson();
        } else if (g_screen == SCREEN_ACCOUNT) {
            int max_items = g_account.signedIn ? 3 : 2;
            if (down & HidNpadButton_Up) g_menu_selection = (g_menu_selection + max_items - 1) % max_items;
            if (down & HidNpadButton_Down) g_menu_selection = (g_menu_selection + 1) % max_items;
            if (down & HidNpadButton_B) g_screen = SCREEN_TIMETABLE;
            if (down & HidNpadButton_A) {
                if (!g_account.signedIn) {
                    if (g_menu_selection == 0) do_login();
                    else g_screen = SCREEN_TIMETABLE;
                } else {
                    if (g_menu_selection == 0) {
                        if (g_account.isAdmin) {
                            load_maintenance();
                            g_screen = SCREEN_ADMIN;
                            g_menu_selection = 0;
                        } else snprintf(g_status, sizeof(g_status), "Admin-Rechte sind fuer dieses Konto nicht aktiv.");
                    } else if (g_menu_selection == 1) {
                        logout_account();
                        g_menu_selection = 0;
                    } else g_screen = SCREEN_TIMETABLE;
                }
            }
            draw_account();
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
    if (g_socket_initialized) socketExit();
    consoleExit(NULL);
    return 0;
}
