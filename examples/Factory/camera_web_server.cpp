#include "camera_web_server.h"

#include <Arduino.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include <img_converters.h>

namespace {

constexpr char STREAM_CONTENT_TYPE[] =
    "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
constexpr char STREAM_SEPARATOR[] =
    "\r\n--123456789000000000000987654321\r\n";
constexpr char STREAM_PART[] =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>T-SimCam Factory Camera</title>
  <style>
    body { margin: 0; background: #111; color: #eee; font-family: Arial, sans-serif; text-align: center; }
    header { padding: 14px; font-size: 20px; font-weight: 700; }
    img { display: block; width: 100%; height: calc(100vh - 52px); object-fit: contain; background: #000; }
  </style>
</head>
<body>
  <header>T-SimCam Factory Camera</header>
  <img id="stream" alt="Camera stream">
  <script>document.getElementById('stream').src='http://'+location.hostname+':81/stream';</script>
</body>
</html>
)HTML";

httpd_handle_t pageServer = nullptr;
httpd_handle_t streamServer = nullptr;

esp_err_t pageHandler(httpd_req_t *request) {
  httpd_resp_set_type(request, "text/html");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t streamHandler(httpd_req_t *request) {
  esp_err_t result = httpd_resp_set_type(request, STREAM_CONTENT_TYPE);
  if (result != ESP_OK) return result;
  httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");

  while (true) {
    camera_fb_t *frame = esp_camera_fb_get();
    if (!frame) return ESP_FAIL;

    uint8_t *jpegBuffer = frame->buf;
    size_t jpegLength = frame->len;
    bool converted = false;
    if (frame->format != PIXFORMAT_JPEG) {
      converted = frame2jpg(frame, 80, &jpegBuffer, &jpegLength);
      if (!converted) {
        esp_camera_fb_return(frame);
        return ESP_FAIL;
      }
    }

    char partHeader[96];
    const size_t headerLength = snprintf(
        partHeader, sizeof(partHeader), STREAM_PART,
        static_cast<unsigned int>(jpegLength));
    result = httpd_resp_send_chunk(request, STREAM_SEPARATOR,
                                   strlen(STREAM_SEPARATOR));
    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(request, partHeader, headerLength);
    }
    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(
          request, reinterpret_cast<const char *>(jpegBuffer), jpegLength);
    }

    if (converted) free(jpegBuffer);
    esp_camera_fb_return(frame);
    if (result != ESP_OK) break;
  }
  return result;
}

}  // namespace

bool startCameraWebServer() {
  httpd_config_t pageConfig = HTTPD_DEFAULT_CONFIG();
  pageConfig.server_port = 80;

  httpd_uri_t pageUri = {};
  pageUri.uri = "/";
  pageUri.method = HTTP_GET;
  pageUri.handler = pageHandler;

  if (httpd_start(&pageServer, &pageConfig) != ESP_OK) return false;
  if (httpd_register_uri_handler(pageServer, &pageUri) != ESP_OK) {
    httpd_stop(pageServer);
    pageServer = nullptr;
    return false;
  }

  httpd_config_t streamConfig = HTTPD_DEFAULT_CONFIG();
  streamConfig.server_port = 81;
  streamConfig.ctrl_port = pageConfig.ctrl_port + 1;

  httpd_uri_t streamUri = {};
  streamUri.uri = "/stream";
  streamUri.method = HTTP_GET;
  streamUri.handler = streamHandler;

  if (httpd_start(&streamServer, &streamConfig) != ESP_OK ||
      httpd_register_uri_handler(streamServer, &streamUri) != ESP_OK) {
    if (streamServer) {
      httpd_stop(streamServer);
      streamServer = nullptr;
    }
    httpd_stop(pageServer);
    pageServer = nullptr;
    return false;
  }
  return true;
}
