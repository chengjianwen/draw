/*
 * a websocket server for drawing, based on wsServer.
 *
 * wsServer is a tiny, lightweight WebSocket server library written 
 * in C that intends to be easy to use, fast, hackable, and compliant
 * to the [RFC 6455](https://tools.ietf.org/html/rfc6455).
 */

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <mypaint-brush.h>
#include <mypaint-fixed-tiled-surface.h>
#include <json-c/json.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <ws.h>

//#define DISABLE_VERBOSE 1

MyPaintBrush *brush;
json_object  *users;
bool         recording;

static void dumpuser()
{
  FILE *fp = fopen ("/tmp/draw/data/user.json", "w");
  if (fp)
  {
    fputs(json_object_to_json_string(users), fp);
    fclose (fp);
  }
}

static void dumpstroke(json_object *obj)
{
  char buf[64];
  int index;
  for (index = 0; index < 10000; index++)
  {
    struct stat   buffer; 
    sprintf (buf, "/tmp/draw/data/stroke_%04d.json", index);
    if (stat (buf, &buffer) != 0)
      break;
  }
  if (index == 10000)
  {
    fprintf (stderr, "too more stroke files, please remove/backup them first!\n");
    return;
  }

  FILE *fp = fopen (buf, "w");
  if (fp)
  {
    fputs(json_object_to_json_string(obj), fp);
    fclose (fp);
  }
}

static void dumpmedia(const char *msg, int size)
{
  char buf[64];
  int index;
  for (index = 0; index < 10000; index++)
  {
    struct stat   buffer; 
    sprintf (buf, "/tmp/draw/data/media_%04d.pcm", index);
    if (stat (buf, &buffer) != 0
    || time(NULL) - buffer.st_mtime < 5)
      break;
  }
  if (index == 10000)
  {
    fprintf (stderr, "too more media files, please remove/backup them first!\n");
    return;
  }

  FILE *fp = fopen (buf, "a");
  if (fp)
  {
    fwrite(msg, 1, size, fp);
    fclose (fp);
  }
}

/*
 * 生成media.json文件
 */
static void dumpmedia2()
{
  json_object *obj;
  obj = json_object_new_array();
  char buf[64];
  int index;
  for (index = 0; index < 10000; index++)
  {
    struct stat   buffer; 
    sprintf (buf, "/tmp/draw/data/media_%04d.pcm", index);
    if (stat (buf, &buffer) < 0)
      break;
    sprintf (buf, "media_%04d", index);
    json_object_array_add(obj, 
                          json_object_new_string((char *)buf));
  }
  FILE *fp = fopen ("/tmp/draw/data/media.json", "w");
  if (fp)
  {
    fputs(json_object_to_json_string(obj), fp);
    fclose (fp);
  }
  json_object_put(obj);
}

static void *playstroke(void *data)
{
  json_object *obj = (json_object *)data;
  /*
  json_object *obj_tmp;
  json_object_object_get_ex(obj, "name", &obj_tmp);
  FILE *fp = fopen (json_object_get_string(obj_tmp), "r");
  */
  json_object_put (obj);
  return NULL;
}

static void *playmedia(void *data)
{
  json_object *obj = (json_object *)data;
  json_object *obj_tmp;
  json_object_object_get_ex(obj, "name", &obj_tmp);
  char buf[8192];
  sprintf (buf,
           "/tmp/draw/data/%s.pcm",
           json_object_get_string(obj_tmp));

  FILE *fp = fopen (buf,
                    "r");
  if (fp)
  {
    json_object_object_get_ex(obj, "fd", &obj_tmp);
    // send media to broadcast
    struct timespec startTime;
    clock_gettime(CLOCK_REALTIME, &startTime);
    while (fread(buf, 1, 8192, fp) == 8192)
    {
      ws_sendframe_bin(json_object_get_int(obj_tmp),
                       (const char *)buf,
                       8192,
                       2);
      struct timespec now, delay;
      clock_gettime(CLOCK_REALTIME, &now);
      startTime.tv_nsec += 512 * 1000000L;
      startTime.tv_sec += startTime.tv_nsec / 1000000000L;
      startTime.tv_nsec = startTime.tv_nsec % 1000000000L;
      delay.tv_sec = startTime.tv_sec - now.tv_sec;
      delay.tv_nsec = startTime.tv_nsec - now.tv_nsec;
      if (delay.tv_nsec < 0)
      {
        delay.tv_sec -= 1;
        delay.tv_nsec += 1.0e9;
      }
      printf ("starttime: %ld sec, %ldnsec\n", startTime.tv_sec, startTime.tv_nsec);
      printf ("      now: %ld sec, %ldnsec\n", now.tv_sec, now.tv_nsec);
      printf ("    delay: %ld sec, %ldnsec\n", delay.tv_sec, delay.tv_nsec);
      nanosleep(&delay, NULL);
    }
    fclose (fp);
  }
  json_object_put (obj);
  return NULL;
}

void onopen(int fd)
{
    char *cli;
        cli = ws_getaddress(fd);
#ifndef DISABLE_VERBOSE
    printf("Connection opened, client: %d | addr: %s\n", fd, cli);
#endif
    json_object *obj;
    obj = json_object_new_object();
    json_object_object_add (obj,
                            "ip",
                             json_object_new_string(cli));
    json_object_object_add (obj,
                            "fd",
                             json_object_new_int(fd));
    json_object_object_add (obj,
                            "time",
                             json_object_new_int64(time(NULL)));
    json_object_array_add (users,
                           obj);
    dumpuser();
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
    json_object *new_users, *obj, *obj_tmp;
    new_users = json_object_new_array();
    for (int i = 0;
         i < json_object_array_length(users);
         i++)
    {
      obj = json_object_array_get_idx(users, i);
      json_object_object_get_ex(obj, "fd", &obj_tmp);
      if (fd != json_object_get_int(obj_tmp))
      {
        json_object_array_add (new_users,
                               obj);
      }
    }
    json_object_put(users);
    users = new_users;
    dumpuser();
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

  if (!size)
  {
#ifndef DISABLE_VERBOSE
    printf("I receive a empty message.\n");
#endif
  }

  switch (type)
  {
    case WS_FR_OP_TXT:
#ifndef DISABLE_VERBOSE
      printf("I receive a message: %s (size: %" PRId64 "), from: %s/%d\n",
      msg, size, cli, fd);
#endif
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
        json_object *obj_tmp_tmp;
        obj_tmp_tmp = json_object_new_object();
        json_object_object_add (obj_tmp_tmp,
                                "action",
                                json_object_new_string("pong"));
        ws_sendframe_txt(fd,
                         json_object_to_json_string(obj_tmp_tmp),
                         2);
        json_object_put(obj_tmp_tmp);
      }
      else if (strcmp(action, "clear") == 0)
      { 
        ws_sendframe_txt(fd,
                         json_object_to_json_string(obj),
                         1);
      }
      else if (strcmp(action, "start") == 0)
      { 
        recording = true;
        ws_sendframe_txt(fd,
                         json_object_to_json_string(obj),
                         1);
      }
      else if (strcmp(action, "stop") == 0)
      { 
        recording = false;
        dumpmedia2();
      }
      else if (strcmp(action, "media") == 0)
      { 
        json_object *obj_tmp_tmp, *obj_tmp_tmp_tmp;
        json_object_object_get_ex(obj,
                                  "name",
                                   &obj_tmp_tmp);
        obj_tmp_tmp_tmp = json_object_new_object();
	json_object_object_add (obj_tmp_tmp_tmp,
                                "fd",
                                json_object_new_int(fd));
	json_object_object_add (obj_tmp_tmp_tmp,
                                "name",
                                json_object_get(obj_tmp_tmp));
        // send start
        ws_sendframe_txt(fd,
                         "{\"action\": \"start\"}",
                         2);
        /*
	pthread_t  pthread_media, pthread_stroke;
        // send media
	pthread_create(&pthread_media,
                       NULL,
                       playmedia,
                       (void *)json_object_get(obj_tmp));
        */
        playmedia((void *)obj_tmp_tmp_tmp);
        // send stroke
	/*
	pthread_create(&pthread_stroke,
                       NULL,
                       playstroke,
                       (void *)json_object_get(obj_tmp));
        */
      }
      else if (strcmp(action, "nick") == 0)
      { 
        json_object *obj_tmp_tmp, *obj_tmp_tmp_tmp;
        for (int i = 0;
             i < json_object_array_length(users);
             i++)
        {
           obj_tmp_tmp = json_object_array_get_idx(users, i);
           json_object_object_get_ex(obj_tmp,
                                     "fd",
                                     &obj_tmp_tmp_tmp);
           if (fd == json_object_get_int(obj_tmp_tmp_tmp))
           {
              json_object_object_get_ex(obj,
			                "nickname",
					&obj_tmp_tmp_tmp);
              json_object_object_add (obj_tmp_tmp,
                                      "nick",
                                      obj_tmp_tmp_tmp);
              break;
           }
        }
        dumpuser();
      }
      else if (strcmp(action, "brush") == 0)
      { 
        json_object *obj_tmp_tmp;
        if (json_object_object_get_ex(obj,
                                      "brush",
                                      &obj_tmp_tmp))
          mypaint_brush_from_string(brush,
                                    json_object_to_json_string(obj_tmp_tmp));
      }
      else if (strcmp(action, "stroke") == 0)
      {
        json_object *obj_tmp_tmp;
        if (json_object_object_get_ex(obj,
                                      "width",
                                      &obj_tmp_tmp))
          width = json_object_get_int(obj_tmp_tmp);
        if (json_object_object_get_ex(obj,
                                      "height",
                                      &obj_tmp_tmp))
          height = json_object_get_int(obj_tmp_tmp);
        if (json_object_object_get_ex(obj,
                                      "stroke",
                                      &obj_tmp_tmp))
        {
          MyPaintSurface *surface = (MyPaintSurface *) mypaint_fixed_tiled_surface_new(width, height);
          MyPaintTiledSurface *tiled_surface = (MyPaintTiledSurface *)surface;
          mypaint_brush_new_stroke (brush);
          mypaint_brush_reset (brush);
          mypaint_surface_begin_atomic (surface);
          for (size_t i = 0;
               i <  json_object_array_length(obj_tmp_tmp);
               i++)
          {
            json_object *obj_tmp_tmp_tmp;
            obj_tmp_tmp_tmp = json_object_array_get_idx(obj_tmp_tmp,
                                                        i);
            json_object *obj_tmp_tmp_tmp_tmp;
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
            long long   last;
            if (json_object_object_get_ex(obj_tmp_tmp_tmp,
                                          "x",
                                          &obj_tmp_tmp_tmp_tmp))
              x = json_object_get_double(obj_tmp_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp_tmp,
                                          "y",
                                          &obj_tmp_tmp_tmp_tmp))
              y = json_object_get_double(obj_tmp_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp_tmp,
                                          "pressure",
                                          &obj_tmp_tmp_tmp_tmp))
              pressure = json_object_get_double(obj_tmp_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp_tmp,
                                          "xtilt",
                                          &obj_tmp_tmp_tmp_tmp))
              xtilt = json_object_get_double(obj_tmp_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp_tmp,
                                          "ytilt",
                                          &obj_tmp_tmp_tmp_tmp))
              ytilt = json_object_get_double(obj_tmp_tmp_tmp_tmp);
            if (json_object_object_get_ex(obj_tmp_tmp_tmp,
                                          "time",
                                          &obj_tmp_tmp_tmp_tmp))
            {
              if (i == 0)
                dtime = 0.0;
              else
                dtime = (float)(json_object_get_int64(obj_tmp_tmp_tmp_tmp) - last) / 1000;
              last = (float)json_object_get_int64(obj_tmp_tmp_tmp_tmp);
	    }
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
            ws_sendframe_txt(fd,
                             json_object_to_json_string(out),
                             2);
            json_object_put(out);
          }
          mypaint_surface_unref(surface);
	  if (recording)
	  {
            dumpstroke(obj_tmp);
	  }
        }
      }
      json_object_put(obj);
      break;
    case WS_FR_OP_BIN:
#ifndef DISABLE_VERBOSE
      printf("I receive a binary message: (size: %" PRId64 "), from: %s/%d\n",
      size, cli, fd);
#endif
      ws_sendframe_bin(fd, 
                   (const char *)msg,
                   size,
                   1);
      if (recording)
      {
        dumpmedia((const char *)msg, size);
      }
      break;
    default:
      break;
  }
  free (cli);

}

static void terminate()
{
  exit(0);
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

  users = json_object_new_array();
  dumpuser();

  recording = false;
  /*
    WebSocket
  */
  struct ws_events *evs;

  evs = (struct ws_events *)malloc (sizeof (struct ws_events));
  evs->onopen    = &onopen;
  evs->onclose   = &onclose;
  evs->onmessage = &onmessage;

  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGKILL, terminate);

  ws_socket(evs, port, 0);
  free (evs);

  return (0);
}
