/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wallpaper uploader - WiFi AP + HTTP server implementation
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_littlefs.h"

#include "wallpaper_uploader.h"

static const char *TAG = "wp_upload";

#define MAX_FILE_SIZE  (5 * 1024 * 1024)   // 5MB per file

// URL decode: convert %XX sequences to their character equivalents
static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t i = 0;
    while (*src && i < dst_size - 1) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = { src[1], src[2], 0 };
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}
#define MAX_OPEN_FILES 32
#define SCRATCH_BUFSIZE (1024)

static httpd_handle_t s_server = NULL;
static esp_netif_t *s_ap_netif = NULL;
static SemaphoreHandle_t s_busy_mutex = NULL;
static bool s_busy = false;
static char s_storage_root[64] = "/storage";
static char s_wallpaper_dir[80] = "/storage/wallpapers";

// Embedded HTML page - the upload UI
static const char index_html[] =
"<!DOCTYPE html>"
"<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Wallpaper Upload</title>"
"<style>"
"body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:20px}"
"h1{text-align:center;color:#00d4ff;font-size:24px}"
".box{background:#16213e;border-radius:12px;padding:20px;margin:15px 0}"
"input[type=file]{background:#0f3460;padding:10px;border-radius:8px;width:100%;box-sizing:border-box;color:#fff}"
"input[type=submit]{background:#00d4ff;color:#000;border:none;padding:12px;border-radius:8px;font-size:16px;width:100%;margin-top:10px;cursor:pointer;font-weight:bold}"
".progress{height:6px;background:#0f3460;border-radius:3px;margin-top:10px;overflow:hidden;display:none}"
".bar{height:100%;background:#00d4ff;width:0%;transition:width .2s}"
".file{background:#0f3460;padding:8px 12px;margin:5px 0;border-radius:6px;display:flex;justify-content:space-between}"
".del{background:#e94560;color:#fff;border:none;padding:4px 10px;border-radius:4px;cursor:pointer}"
"#msg{min-height:20px;text-align:center;margin-top:10px}"
"#count{text-align:center;color:#00d4ff;font-size:14px}"
"</style></head><body>"
"<h1>Wallpaper Upload</h1>"
"<div class='box'>"
"<form id='f'><input type='file' name='f' id='fi' accept='.jpg,.jpeg,.png,.bmp,.gif' multiple>"
"<input type='submit' value='Upload'></form>"
"<div class='progress' id='pg'><div class='bar' id='pb'></div></div>"
"<div id='msg'></div>"
"</div>"
"<div class='box'>"
"<div id='count'></div>"
"<button class='del' style='width:100%;margin-bottom:10px' onclick='delAll()'>Delete All</button>"
"<div id='list'></div>"
"</div>"
"<script>"
"var fi=document.getElementById('fi'),f=document.getElementById('f'),msg=document.getElementById('msg'),"
"pg=document.getElementById('pg'),pb=document.getElementById('pb'),list=document.getElementById('list'),cnt=document.getElementById('count');"
"var MAX_SIZE=466;"
"function refresh(){fetch('/list').then(r=>r.json()).then(j=>{"
"cnt.textContent=j.files.length+' wallpapers ('+(j.total/1024).toFixed(1)+' KB)';"
"list.innerHTML=j.files.map(n=>'<div class=\"file\"><span>'+n+'</span><button class=\"del\" onclick=\"del(\\''+n+'\\')\">X</button></div>').join('');"
"})}refresh();"
"function del(n){if(!confirm('Delete '+n+'?'))return;fetch('/delete?name='+encodeURIComponent(n),{method:'POST'}).then(refresh)}"
"function delAll(){if(!confirm('Delete ALL wallpapers?'))return;fetch('/list').then(r=>r.json()).then(j=>{"
"j.files.reduce((p,n)=>p.then(()=>fetch('/delete?name='+encodeURIComponent(n),{method:'POST'})),Promise.resolve()).then(refresh)"
"})}"
"function resizeImage(file){return new Promise(function(resolve){"
"  if(file.name.toLowerCase().endsWith('.gif')){resolve(file);return;}"
"  var img=new Image();"
"  img.onload=function(){"
"    var w=img.width,h=img.height;"
"    if(w<=MAX_SIZE&&h<=MAX_SIZE){resolve(file);return;}"
"    var scale=Math.min(MAX_SIZE/w,MAX_SIZE/h);"
"    var nw=Math.round(w*scale),nh=Math.round(h*scale);"
"    var c=document.createElement('canvas');"
"    c.width=nw;c.height=nh;"
"    var ctx=c.getContext('2d');"
"    ctx.drawImage(img,0,0,nw,nh);"
"    c.toBlob(function(blob){"
"      var newName=file.name.replace(/\\.[^.]+$/,'.jpg');"
"      resolve(new File([blob],newName,{type:'image/jpeg'}));"
"    },'image/jpeg',0.9);"
"  };"
"  img.onerror=function(){resolve(file);};"
"  img.src=URL.createObjectURL(file);"
"})}"
"async function uploadAll(e){"
"  if(e&&e.preventDefault)e.preventDefault();"
"  if(!fi.files.length)return;"
"  msg.textContent='Processing...';"
"  pg.style.display='block';pb.style.width='0%';"
"  var files=[];"
"  for(var i=0;i<fi.files.length;i++){"
"    var isGif=fi.files[i].name.toLowerCase().endsWith('.gif');"
"    msg.textContent=isGif?'Preparing GIF...':'Resizing '+(i+1)+'/'+fi.files.length+'...';"
"    files.push(await resizeImage(fi.files[i]));"
"  }"
"  msg.textContent='Uploading...';"
"  for(var i=0;i<files.length;i++){"
"    var file=files[i];"
"    await new Promise(function(done){"
"      var r=new XMLHttpRequest();"
"      r.open('POST','/upload?name='+encodeURIComponent(file.name));"
"      r.upload.onprogress=function(ev){"
"        if(ev.lengthComputable){"
"          var p=(i+ev.loaded/ev.total)/files.length*100;"
"          pb.style.width=p+'%';"
"        }"
"      };"
"      r.onerror=function(){msg.textContent='Err '+r.status;done();};"
"      r.onload=function(){if(r.status!=200)msg.textContent='Err '+r.status;done();};"
"      r.send(file);"
"    });"
"  }"
"  pg.style.display='none';"
"  msg.textContent='Done!';"
"  refresh();"
"}"
"f.addEventListener('submit',uploadAll);"
"</script></body></html>";

// ============= Filesystem helpers =============

static esp_err_t ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return (S_ISDIR(st.st_mode)) ? ESP_OK : ESP_FAIL;
    }
    if (mkdir(path, 0755) == 0) {
        ESP_LOGI(TAG, "Created directory: %s", path);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "mkdir(%s) failed: errno=%d", path, errno);
    return ESP_FAIL;
}

// Tiny memmem replacement - newlib on ESP-IDF doesn't always export it.
static void *wp_memmem(const void *hay, size_t haylen, const void *needle, size_t needlelen)
{
    if (needlelen == 0) return (void *)hay;
    if (haylen < needlelen) return NULL;
    const char *h = (const char *)hay;
    const char *n = (const char *)needle;
    for (size_t i = 0; i + needlelen <= haylen; i++) {
        if (memcmp(h + i, n, needlelen) == 0) return (void *)(h + i);
    }
    return NULL;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t list_handler(httpd_req_t *req)
{
    DIR *dir = opendir(s_wallpaper_dir);
    if (!dir) {
        // Return empty list
        const char *empty = "{\"files\":[],\"total\":0}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, empty, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    // Build JSON manually - avoid malloc per file
    char buf[2048];
    size_t pos = 0;
    size_t total = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"files\":[");
    bool first = true;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        // GCC's -Wformat-truncation warns because it conservatively
        // assumes entry->d_name can be NAME_MAX bytes; our buffers are
        // sized for the realistic maximum path on this filesystem, so
        // silence the false positive locally.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        char full[160];
        snprintf(full, sizeof(full), "%s/%s", s_wallpaper_dir, entry->d_name);
#pragma GCC diagnostic pop
        struct stat st;
        if (stat(full, &st) == 0) {
            total += st.st_size;
        }
        if (!first) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        // escape quotes if needed (skip for now - filenames assumed safe)
        pos += snprintf(buf + pos, sizeof(buf) - pos, "\"%s\"", entry->d_name);
        first = false;
    }
    closedir(dir);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "],\"total\":%zu}", total);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, pos);
    return ESP_OK;
}

static esp_err_t delete_handler(httpd_req_t *req)
{
    char query[256];
    int qlen = httpd_req_get_url_query_len(req);
    ESP_LOGI(TAG, "delete: query_len=%d", qlen);
    if (qlen > 0 && httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        ESP_LOGI(TAG, "delete: query='%s'", query);
        char name_encoded[128] = {0};
        if (httpd_query_key_value(query, "name", name_encoded, sizeof(name_encoded)) == ESP_OK) {
            // URL decode the filename
            char name[128] = {0};
            url_decode(name, name_encoded, sizeof(name));
            ESP_LOGI(TAG, "delete: name='%s'", name);
            // Sanitize - reject ".." and "/"
            if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad name");
                return ESP_FAIL;
            }
            char path[160];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(path, sizeof(path), "%s/%s", s_wallpaper_dir, name);
#pragma GCC diagnostic pop
            if (unlink(path) == 0) {
                ESP_LOGI(TAG, "Deleted %s", path);
                httpd_resp_sendstr(req, "OK");
                return ESP_OK;
            }
            ESP_LOGE(TAG, "unlink(%s) failed errno=%d", path, errno);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
            return ESP_FAIL;
        }
        ESP_LOGW(TAG, "delete: 'name' not in query");
    } else {
        ESP_LOGW(TAG, "delete: no query string");
    }
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing name");
    return ESP_FAIL;
}

static esp_err_t upload_post_handler(httpd_req_t *req)
{
    if (s_busy) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Busy");
        return ESP_FAIL;
    }
    xSemaphoreTake(s_busy_mutex, portMAX_DELAY);
    s_busy = true;
    xSemaphoreGive(s_busy_mutex);

    int remaining = req->content_len;
    if (remaining <= 0 || remaining > MAX_FILE_SIZE) {
        s_busy = false;
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Bad size");
        return ESP_FAIL;
    }

    // Filename comes from the URL query string: POST /upload?name=foo.jpg
    char query[128] = {0};
    char name_encoded[128] = {0};
    char name[128] = {0};
    if (httpd_req_get_url_query_len(req) > 0 &&
        httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "name", name_encoded, sizeof(name_encoded)) == ESP_OK) {
        // URL decode the filename (handles Chinese characters like %E5%BE%AE...)
        url_decode(name, name_encoded, sizeof(name));
    }
    if (name[0] == 0) {
        s_busy = false;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing name");
        return ESP_FAIL;
    }
    // Sanitize: reject ".." and path separators
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) {
        s_busy = false;
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad name");
        return ESP_FAIL;
    }

    char outname[256];
    snprintf(outname, sizeof(outname), "%s/%s", s_wallpaper_dir, name);

    ESP_LOGI(TAG, "Upload: %d bytes -> %s", remaining, outname);

    FILE *f = fopen(outname, "wb");
    if (!f) {
        s_busy = false;
        ESP_LOGE(TAG, "Cannot create %s", outname);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
        return ESP_FAIL;
    }

    // Stream the request body straight to disk. No multipart parsing.
    char buf[SCRATCH_BUFSIZE];
    int saved = 0;
    while (remaining > 0) {
        int want = remaining > SCRATCH_BUFSIZE ? SCRATCH_BUFSIZE : remaining;
        int n = httpd_req_recv(req, buf, want);
        if (n <= 0) {
            fclose(f);
            unlink(outname);
            s_busy = false;
            ESP_LOGE(TAG, "Recv failed: %d", n);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
            return ESP_FAIL;
        }
        if (fwrite(buf, 1, n, f) != (size_t)n) {
            fclose(f);
            unlink(outname);
            s_busy = false;
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ESP_FAIL;
        }
        saved += n;
        remaining -= n;
    }
    fclose(f);

    ESP_LOGI(TAG, "Saved %s (%d bytes)", outname, saved);
    httpd_resp_sendstr(req, "OK");
    s_busy = false;
    return ESP_OK;
}

// ============= WiFi AP setup =============

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

// ============= Public API =============

bool wallpaper_uploader_start(const wallpaper_uploader_config_t *config)
{
    if (!config) return false;
    if (s_server) {
        ESP_LOGW(TAG, "Already started");
        return true;
    }

    s_busy_mutex = xSemaphoreCreateMutex();
    if (!s_busy_mutex) return false;

    strncpy(s_storage_root, config->storage_root, sizeof(s_storage_root) - 1);
    snprintf(s_wallpaper_dir, sizeof(s_wallpaper_dir), "%s/wallpapers", config->storage_root);

    // Ensure wallpaper directory exists
    if (ensure_dir(s_storage_root) != ESP_OK) return false;
    if (ensure_dir(s_wallpaper_dir) != ESP_OK) return false;

    // Init netif (idempotent if main already did this)
    esp_netif_init();
    esp_event_loop_create_default();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.ap.ssid, config->ap_ssid, sizeof(wifi_config.ap.ssid));
    if (config->ap_password && strlen(config->ap_password) >= 8) {
        strlcpy((char *)wifi_config.ap.password, config->ap_password, sizeof(wifi_config.ap.password));
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    wifi_config.ap.ssid_len = strlen(config->ap_ssid);
    wifi_config.ap.channel = config->channel;
    wifi_config.ap.max_connection = config->max_connections;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started: SSID=%s password=%s channel=%d",
             config->ap_ssid, config->ap_password ?: "(open)", config->channel);

    // Start HTTP server
    httpd_config_t server_cfg = HTTPD_DEFAULT_CONFIG();
    server_cfg.max_uri_handlers = 8;
    server_cfg.stack_size = 8192;
    // LWIP_MAX_SOCKETS on the 1.75C without PSRAM is small; the HTTP
    // server itself reserves 3 sockets internally and the LWIP stack
    // caps the total. 4 client sockets is plenty for an upload UI.
    server_cfg.max_open_sockets = 4;
    server_cfg.lru_purge_enable = true;

    if (httpd_start(&s_server, &server_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return false;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &index_uri);

    httpd_uri_t list_uri = {
        .uri = "/list",
        .method = HTTP_GET,
        .handler = list_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &list_uri);

    httpd_uri_t upload_uri = {
        .uri = "/upload",
        .method = HTTP_POST,
        .handler = upload_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &upload_uri);

    httpd_uri_t delete_uri = {
        .uri = "/delete",
        .method = HTTP_POST,
        .handler = delete_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &delete_uri);

    ESP_LOGI(TAG, "HTTP server started on http://192.168.4.1/");
    return true;
}

void wallpaper_uploader_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    if (s_busy_mutex) {
        vSemaphoreDelete(s_busy_mutex);
        s_busy_mutex = NULL;
    }
    ESP_LOGI(TAG, "Stopped");
}

bool wallpaper_uploader_is_busy(void)
{
    bool b = false;
    if (s_busy_mutex) {
        xSemaphoreTake(s_busy_mutex, portMAX_DELAY);
        b = s_busy;
        xSemaphoreGive(s_busy_mutex);
    }
    return b;
}

size_t wallpaper_uploader_get_file_count(void)
{
    DIR *dir = opendir(s_wallpaper_dir);
    if (!dir) return 0;
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') count++;
    }
    closedir(dir);
    return count;
}