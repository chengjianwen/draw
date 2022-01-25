/*
 * a websocket server for drawing, based on wsServer.
 *
 * wsServer is a tiny, lightweight WebSocket server library written 
 * in C that intends to be easy to use, fast, hackable, and compliant
 * to the [RFC 6455](https://tools.ietf.org/html/rfc6455).
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <mypaint-brush.h>
#include <mypaint-fixed-tiled-surface.h>
#include <json-c/json.h>
#include <math.h>
#include <sys/time.h>
#include <ws.h>

#define DISABLE_VERBOSE 1
/*
 * global variable
 */
MyPaintBrush *brush;

/**
 * @dir example/
 * @brief wsServer examples folder
 *
 * @file send_receive.c
 * @brief Simple send/receiver example.
 */

/**
 * @brief Called when a client connects to the server.
 *
 * @param fd File Descriptor belonging to the client. The @p fd parameter
 * is used in order to send messages and retrieve informations
 * about the client.
 */
void onopen(int fd)
{
    char *cli;
    cli = ws_getaddress(fd);
#ifndef DISABLE_VERBOSE
    printf("Connection opened, client: %d | addr: %s\n", fd, cli);
#endif
    free(cli);
}

/**
 * @brief Called when a client disconnects to the server.
 *
 * @param fd File Descriptor belonging to the client. The @p fd parameter
 * is used in order to send messages and retrieve informations
 * about the client.
 */
void onclose(int fd)
{
    char *cli;
    cli = ws_getaddress(fd);
#ifndef DISABLE_VERBOSE
    printf("Connection closed, client: %d | addr: %s\n", fd, cli);
#endif
    free(cli);
}

/**
 * @brief Called when a client connects to the server.
 *
 * @param fd File Descriptor belonging to the client. The
 * @p fd parameter is used in order to send messages and
 * retrieve informations about the client.
 *
 * @param msg Received message, this message can be a text
 * or binary message.
 *
 * @param size Message size (in bytes).
 *
 * @param type Message type.
 */
void onmessage(int fd, const unsigned char *msg, uint64_t size, int type)
{
  char *cli;
  cli = ws_getaddress(fd);
#ifndef DISABLE_VERBOSE
  printf("I receive a message: %s (size: %" PRId64 ", type: %d), from: %s/%d\n",
      msg, size, type, cli, fd);
#endif
  free(cli);

  if (!size)
  {
#ifndef DISABLE_VERBOSE
    printf("I receive a empty message.\n");
#endif
    return;
  }

  switch (type)
  {
    case WS_FR_OP_TXT:
      ;
      json_object *obj;
      obj = json_tokener_parse((char *)msg);
      int width, height;
      const char *action;
  
      json_object *obj_tmp;
      if (json_object_object_get_ex(obj,
                                    "action",
                                    &obj_tmp))
        action = json_object_get_string (obj_tmp);
      if (strcmp(action, "ping") == 0)
      { 
        obj_tmp = json_object_new_object();
        json_object_object_add (obj_tmp,
                                "action",
                                json_object_new_string("pong"));
        ws_sendframe(fd,
                     json_object_to_json_string(obj_tmp),
                     strlen(json_object_to_json_string(obj_tmp)),
                     true,
                     type);
        json_object_put(obj_tmp);
      }
      else if (strcmp(action, "clear") == 0)
      { 
        ws_sendframe(fd,
                     json_object_to_json_string(obj),
                     strlen(json_object_to_json_string(obj)),
                     true,
                     type);
      }
      else if (strcmp(action, "brush") == 0)
      { 
        if (json_object_object_get_ex(obj,
                                      "brush",
                                      &obj_tmp))
          mypaint_brush_from_string(brush,
                                    json_object_to_json_string(obj_tmp));
      }
      else if (strcmp(action, "stroke") == 0)
      {
        if (json_object_object_get_ex(obj,
                                      "width",
                                      &obj_tmp))
          width = json_object_get_int(obj_tmp);
        if (json_object_object_get_ex(obj,
                                      "height",
                                      &obj_tmp))
          height = json_object_get_int(obj_tmp);
        if (json_object_object_get_ex(obj,
                                      "stroke",
                                      &obj_tmp))
        {
          MyPaintSurface *surface = (MyPaintSurface *) mypaint_fixed_tiled_surface_new(width, height);
          MyPaintTiledSurface *tiled_surface = (MyPaintTiledSurface *)surface;
          mypaint_brush_new_stroke (brush);
          mypaint_brush_reset (brush);
          mypaint_surface_begin_atomic (surface);
          for (unsigned long i = 0;
               i <  json_object_array_length(obj_tmp);
               i++)
          {
            json_object *obj_tmp_tmp;
            obj_tmp_tmp = json_object_array_get_idx(obj_tmp,
                                                    i);
            json_object *obj_tmp_tmp_tmp;
            float            x = 0.0;
            float            y = 0.0;
            float     pressure = 0.5;
            float        xtilt = 0.0;
            float        ytilt = 0.0;
            float        dtime = 0.0;
            float     viewzoom = 1.0;
            float viewrotation = 0.0;
            float barrel_rotation = 0.0;
            int linear = 0;
            if (json_object_object_get_ex(obj_tmp_tmp,
                                          "x",
                                          &obj_tmp_tmp_tmp))
              x = json_object_get_double(obj_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp,
                                          "y",
                                          &obj_tmp_tmp_tmp))
              y = json_object_get_double(obj_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp,
                                          "pressure",
                                          &obj_tmp_tmp_tmp))
              pressure = json_object_get_double(obj_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp,
                                          "xtilt",
                                          &obj_tmp_tmp_tmp))
              xtilt = json_object_get_double(obj_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp,
                                          "ytilt",
                                          &obj_tmp_tmp_tmp))
              ytilt = json_object_get_double(obj_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp,
                                          "dtime",
                                          &obj_tmp_tmp_tmp))
              dtime = (float)json_object_get_double(obj_tmp_tmp_tmp) / 1000;
            mypaint_brush_stroke_to (brush,
                                     surface,
                                     x,
                                     y,
                                     pressure,
                                     xtilt,
                                     ytilt,
                                     dtime,
                                     viewzoom,
                                     viewrotation,
                                     barrel_rotation,
                                     linear);
          }
    
          MyPaintRectangle roi;
          MyPaintRectangles rois;
          rois.num_rectangles = 1;
          rois.rectangles = &roi;
          mypaint_surface_end_atomic(surface,
                                     &rois);
          if (roi.width
          && roi.height)
          {
            json_object *out = json_object_new_object();
            json_object_object_add (out,
                                    "width",
                                    json_object_new_int(width));
            json_object_object_add (out,
                                    "height",
                                    json_object_new_int(height));
            json_object *stroke = json_object_new_array();
            int tiles_height = ceil((float)height / MYPAINT_TILE_SIZE);
            int tiles_width = ceil((float)width / MYPAINT_TILE_SIZE);
            for (int tx = floor((float)roi.x / MYPAINT_TILE_SIZE); tx < ceil((float)(roi.x + roi.width) / MYPAINT_TILE_SIZE); tx++)
            {
                for (int ty = floor((float)roi.y / MYPAINT_TILE_SIZE);
                     ty < ceil((float)(roi.y + roi.height) / MYPAINT_TILE_SIZE);
                     ty++)
                {
                    const int max_x = tx < tiles_width - 1 || width % MYPAINT_TILE_SIZE == 0 ? MYPAINT_TILE_SIZE : width % MYPAINT_TILE_SIZE;
                    const int max_y = ty < tiles_height - 1 || height % MYPAINT_TILE_SIZE == 0 ? MYPAINT_TILE_SIZE : height % MYPAINT_TILE_SIZE;
                    MyPaintTileRequest request;
                    mypaint_tile_request_init(&request,
                                              0,
                                              tx,
                                              ty,
                                              TRUE);
                    mypaint_tiled_surface_tile_request_start(tiled_surface,
                                                             &request);
                    for (int y = 0;
                         y < max_y;
                         y++)
                    {
                        int yy = ty * MYPAINT_TILE_SIZE + y;
                        for (int x = 0;
                             x < max_x;
                             x++)
                        {
                            int xx = tx * MYPAINT_TILE_SIZE + x;
                            int offset = MYPAINT_TILE_SIZE * y + x;
                            unsigned short r = request.buffer[offset * 4];
                            unsigned short g = request.buffer[offset * 4 + 1];
                            unsigned short b = request.buffer[offset * 4 + 2];
                            if (r != 0xFFFF
                            || g != 0xFFFF
                            || b != 0xFFFF)
                            {
                                r &= 0x7FFF;
                                g &= 0x7FFF;
                                b &= 0x7FFF;
                
                                json_object *pixel = json_object_new_array();
                                json_object_array_add (pixel,
                                                       json_object_new_int(xx));
                                json_object_array_add (pixel,
                                                       json_object_new_int(yy));
                                json_object_array_add (pixel,
                                                       json_object_new_int(r));
                                json_object_array_add (pixel,
                                                       json_object_new_int(g));
                                json_object_array_add (pixel,
                                                       json_object_new_int(b));
                                json_object_array_add (stroke,
                                                       pixel);
                            }
                        }
                    }
                    mypaint_tiled_surface_tile_request_end(tiled_surface,
                                                           &request);
                }
            }
            json_object_object_add (out,
                                    "stroke",
                                    stroke);
            const char *p;
            p = json_object_to_json_string_ext (out,
                                                JSON_C_TO_STRING_PLAIN);
            ws_sendframe(fd,
                         (const char *)p,
                         strlen(p),
                         true,
                         type);
            json_object_put(out);
          }
          mypaint_surface_unref(surface);
        }
      }
      json_object_put(obj);
      break;
    case WS_FR_OP_BIN:
      ws_sendframe(fd, 
                   (const char *)msg,
                   size,
                   true,
                   type);
      break;
    default:
      break;
  }
}

int main(int argc, char *argv[])
{
    int   c;
    int   port = 8080;
    while ((c = getopt(argc, argv, "p:")) != -1)
    {
    switch (c)
    {
      case 'p':
        port = atoi(optarg);
        break;
      default:
        break;
    }
  }
  /*
    Initialize the brush
  */
  brush = mypaint_brush_new();
  mypaint_brush_from_defaults(brush);

  /*
    WebSocket
  */
  struct ws_events *evs;

  evs = (struct ws_events *)malloc (sizeof (struct ws_events));
  evs->onopen    = &onopen;
  evs->onclose   = &onclose;
  evs->onmessage = &onmessage;
  ws_socket(evs, port, 0);
  free (evs);

  return (0);
}
