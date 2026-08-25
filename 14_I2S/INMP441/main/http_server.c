/**
 * @file http_server.c
 * @brief HTTP server with auto-loop recording page.
 *
 * GET /        – serves an HTML page; JS loops N times: fetch /record → auto-download
 * GET /record  – records once, streams back the WAV file (browser downloads it)
 * GET /delete  – removes the WAV file from LittleFS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "http_server.h"
#include "app_config.h"

static const char *TAG = "HTTP_SRV";

#define STREAM_CHUNK_SIZE 4096

static EventGroupHandle_t s_eg      = NULL;
static SemaphoreHandle_t  s_rec_mtx = NULL;

/* --------------------------------------------------------------------------
 * Embedded HTML page
 *
 * JavaScript flow (runs entirely in the browser):
 *   for i = 1..N:
 *     fetch('/record')          ← ESP32 records + streams WAV back
 *     create <a> and click it   ← browser downloads audio_NNN.wav
 *     await small delay
 * -------------------------------------------------------------------------- */
static const char INDEX_HTML[] =
"<!DOCTYPE html>"
"<html lang='vi'>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>INMP441 Recorder</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:'Segoe UI',sans-serif;background:#0f0f1a;color:#e0e0e0;"
"     display:flex;flex-direction:column;align-items:center;padding:40px 16px}"
"h1{font-size:1.8rem;color:#7eb8f7;margin-bottom:8px}"
"p.sub{color:#888;margin-bottom:32px;font-size:.9rem}"
".card{background:#1a1a2e;border:1px solid #2a2a4a;border-radius:16px;"
"      padding:28px;width:100%;max-width:440px;margin-bottom:20px}"
"label{display:block;color:#aaa;font-size:.85rem;margin-bottom:6px}"
"input[type=number]{width:100%;padding:10px 14px;border-radius:8px;"
"  border:1px solid #3a3a5a;background:#12122a;color:#fff;font-size:1rem}"
"button{display:block;width:100%;padding:13px;margin-top:16px;"
"  border:none;border-radius:10px;background:#4f80e1;color:#fff;"
"  font-size:1rem;font-weight:600;cursor:pointer;transition:background .2s}"
"button:hover:not(:disabled){background:#6a99f5}"
"button:disabled{background:#2a2a4a;color:#555;cursor:not-allowed}"
"#log{background:#12122a;border-radius:10px;padding:16px;min-height:60px;"
"     font-size:.88rem;line-height:1.7;white-space:pre-wrap}"
".ok{color:#4caf82}.err{color:#e06060}.info{color:#7eb8f7}.dim{color:#666}"
"</style>"
"</head>"
"<body>"
"<h1>&#127908; INMP441 Recorder</h1>"
"<p class='sub'>Automatic recording &amp; dataset collection</p>"
"<div class='card'>"
"  <label>Number of recordings</label>"
"  <input type='number' id='n' value='5' min='1' max='99'>"
"  <label style='margin-top:14px'>Filename prefix (prefix_NNN.wav)</label>"
"  <input type='text' id='prefix' value='audio'"
"    style='width:100%;padding:10px 14px;border-radius:8px;"
"           border:1px solid #3a3a5a;background:#12122a;color:#fff;font-size:1rem'>"
"  <button id='btn' onclick='run()'>&#9654; Start Recording</button>"
"</div>"
"<div class='card'>"
"  <div id='log'><span class='dim'>Ready.</span></div>"
"</div>"
"<script>"
"let running=false;"
"function log(html){document.getElementById('log').innerHTML=html;}"
"async function run(){"
"  if(running)return;"
"  running=true;"
"  const btn=document.getElementById('btn');"
"  btn.disabled=true;"
"  const n=Math.max(1,parseInt(document.getElementById('n').value)||1);"
"  const prefix=document.getElementById('prefix').value.trim()||'audio';"
"  let lines=[];"
"  for(let i=1;i<=n;i++){"
"    lines[i-1]=`<span class='info'>&#9654; Recording ${i}/${n}...</span>`;"
"    log(lines.join('\\n'));"
"    try{"
"      const r=await fetch('/record');"
"      if(!r.ok){lines[i-1]=`<span class='err'>&#10007; Error ${i}: HTTP ${r.status}</span>`;log(lines.join('\\n'));break;}"
"      const blob=await r.blob();"
"      const fname=`${prefix}_${String(i).padStart(3,'0')}.wav`;"
"      const a=document.createElement('a');"
"      a.href=URL.createObjectURL(blob);"
"      a.download=fname;"
"      document.body.appendChild(a);a.click();document.body.removeChild(a);"
"      lines[i-1]=`<span class='ok'>&#10003; ${fname}</span>`;"
"    }catch(e){"
"      lines[i-1]=`<span class='err'>&#10007; Error ${i}: ${e.message}</span>`;"
"      log(lines.join('\\n'));break;"
"    }"
"    log(lines.join('\\n'));"
"    if(i<n)await new Promise(r=>setTimeout(r,200));"
"  }"
"  log(lines.join('\\n')+'\\n<span class=\\'ok\\'>&#10003; All done!</span>');"
"  btn.disabled=false;"
"  running=false;"
"}"
"</script>"
"</body></html>";

/* ============================================================
 * Handler: GET /  — serve the HTML control page
 * ============================================================ */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, (ssize_t)sizeof(INDEX_HTML) - 1);
}

/* ============================================================
 * Handler: GET /record
 *
 * Signals recorder_task to record one WAV file, then streams
 * it back to the browser as an attachment (auto-download).
 * ============================================================ */
static esp_err_t record_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /record");

    if (xSemaphoreTake(s_rec_mtx, 0) != pdTRUE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Recording in progress. Retry later.\n");
    }

    xEventGroupClearBits(s_eg, BIT_RECORDING_DONE);
    xEventGroupSetBits(s_eg, BIT_START_RECORDING);

    EventBits_t bits = xEventGroupWaitBits(
            s_eg, BIT_RECORDING_DONE,
            pdTRUE, pdFALSE,
            pdMS_TO_TICKS(RECORD_TOTAL_TIMEOUT_MS));

    xSemaphoreGive(s_rec_mtx);

    if (!(bits & BIT_RECORDING_DONE)) {
        ESP_LOGE(TAG, "/record timed out");
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Recording timed out.\n");
    }

    /* Stream the WAV file back to the browser */
    struct stat st;
    if (stat(WAV_FILE_PATH, &st) != 0) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "File not found after recording.\n");
    }

    FILE *fp = fopen(WAV_FILE_PATH, "rb");
    if (fp == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "Cannot open recorded file.\n");
    }

    httpd_resp_set_type(req, "audio/wav");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"audio.wav\"");
    char len_buf[20];
    snprintf(len_buf, sizeof(len_buf), "%ld", (long)st.st_size);
    httpd_resp_set_hdr(req, "Content-Length", len_buf);

    char *chunk = malloc(STREAM_CHUNK_SIZE);
    if (!chunk) { fclose(fp); return ESP_ERR_NO_MEM; }

    esp_err_t res = ESP_OK;
    size_t n;
    while ((n = fread(chunk, 1, STREAM_CHUNK_SIZE, fp)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, (int)n) != ESP_OK) {
            res = ESP_FAIL; break;
        }
    }
    if (res == ESP_OK) httpd_resp_send_chunk(req, NULL, 0);

    free(chunk);
    fclose(fp);
    ESP_LOGI(TAG, "Streamed %ld bytes", (long)st.st_size);
    return res;
}

/* ============================================================
 * Handler: GET /delete
 * ============================================================ */
static esp_err_t delete_handler(httpd_req_t *req)
{
    if (remove(WAV_FILE_PATH) == 0) {
        return httpd_resp_sendstr(req, "Deleted.\n");
    }
    if (errno == ENOENT) {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_sendstr(req, "No file to delete.\n");
    }
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "Delete failed.\n");
}

/* ============================================================
 * Public: start the HTTP server
 * ============================================================ */
esp_err_t http_server_start(EventGroupHandle_t eg)
{
    if (!eg) return ESP_ERR_INVALID_ARG;
    s_eg = eg;

    s_rec_mtx = xSemaphoreCreateMutex();
    if (!s_rec_mtx) return ESP_ERR_NO_MEM;

    httpd_config_t cfg   = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = HTTP_SERVER_PORT;
    cfg.max_open_sockets = 4;
    cfg.lru_purge_enable = true;

    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &cfg), TAG, "httpd_start failed");

    static const httpd_uri_t u_index  = { .uri="/",       .method=HTTP_GET, .handler=index_handler  };
    static const httpd_uri_t u_record = { .uri="/record", .method=HTTP_GET, .handler=record_handler };
    static const httpd_uri_t u_delete = { .uri="/delete", .method=HTTP_GET, .handler=delete_handler };

    httpd_register_uri_handler(server, &u_index);
    httpd_register_uri_handler(server, &u_record);
    httpd_register_uri_handler(server, &u_delete);

    ESP_LOGI(TAG, "HTTP server on port %d  →  open http://<ESP_IP>/", HTTP_SERVER_PORT);
    return ESP_OK;
}
