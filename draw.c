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
#include <sqlite3.h>
#include <uuid/uuid.h>
#include <ws.h>

//#define DISABLE_VERBOSE 1

MyPaintBrush *brush;
bool         recording;
bool         playing;
pthread_t    pthread_media, pthread_stroke;
sqlite3      *conn;
FILE         *mediafile = NULL;

char *current_timestamp()
{
  struct timespec now;
  struct tm *t;
  static char   *buf = NULL;
  if (!buf)
    buf = malloc(24);
  clock_gettime(CLOCK_REALTIME,
                &now);
  t = localtime (&now.tv_sec);
  sprintf (buf,
           "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           t->tm_year + 1900,
           t->tm_mon + 1,
           t->tm_mday,
           t->tm_hour,
           t->tm_min,
           t->tm_sec,
           now.tv_nsec / 1000000L);
  return buf;
}

static void *playstroke(void *data)
{
  json_object *obj = (json_object *)data;
  json_object *obj_time, *obj_fd;
  if (json_object_object_get_ex(obj, "time", &obj_time)
  && json_object_object_get_ex(obj, "fd", &obj_fd))
  {
    char  sql[1024];
    sprintf (sql,
            "SELECT (JulianDay(DATETIME(time, 'localtime')) - JulianDay('%s')) * 86400 AS delay, stroke FROM STROKE WHERE delay > 0 ORDER BY delay",
            json_object_get_string(obj_time));
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2 (conn,
                        sql,
                        -1,
                        &stmt,
                        NULL);
    struct timespec delay = {0, 0};
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      delay.tv_sec = sqlite3_column_int(stmt, 0) - delay.tv_sec;;
      delay.tv_nsec = (sqlite3_column_double(stmt, 0) - delay.tv_sec) * 1000000000L - delay.tv_nsec;
      if (delay.tv_nsec < 0)
      {
        delay.tv_sec -= 1;
        delay.tv_nsec += 1000000000L;
      }
      nanosleep(&delay, NULL);
      printf ("deley %f\n", sqlite3_column_double(stmt, 0));
      ws_sendframe_txt(json_object_get_int(obj_fd),
                       (char *)sqlite3_column_text(stmt, 1),
                       2);
      if (!playing)
        break;
    }
    sqlite3_finalize (stmt);
    json_object_put (obj);
  }
  return NULL;
}

static void *playmedia(void *data)
{
  json_object *obj, *obj_filename, *obj_fd;
  obj = (json_object *)data;

  if (json_object_object_get_ex(obj,
                                "filename",
                                &obj_filename)
  && json_object_object_get_ex(obj,
                               "fd",
                               &obj_fd))
  {
    FILE *fp = fopen (json_object_get_string(obj_filename),
                      "r");
    if (fp)
    {
      // send media to broadcast
      struct timespec startTime;
      clock_gettime(CLOCK_REALTIME, &startTime);
      char buf[8192];
      while (playing && fread(buf, 1, 8192, fp) == 8192)
      {
        ws_sendframe_bin(json_object_get_int(obj_fd),
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
        nanosleep(&delay, NULL);
      }
      fclose (fp);
    }
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
    free(cli);
    char sql[1024];
    sprintf (sql,
             "INSERT INTO ONLINE (fd, time) VALUES (%d, '%s')",
             fd,
             current_timestamp());
    sqlite3_exec (conn,
                  sql,
                  0,
                  0,
                  0);
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
    char sql[1024];
    sprintf (sql,
             "DELETE FROM ONLINE WHERE fd =  %d",
             fd);
    sqlite3_exec (conn,
                  sql,
                  0,
                  0,
                  0);
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

      json_object *obj_action;
      if (json_object_object_get_ex(obj,
                                    "action",
                                    &obj_action))
      {
        if (strcmp(json_object_get_string (obj_action), "ping") == 0)
        { 
          json_object *obj_pong;
          obj_pong = json_object_new_object();
          json_object_object_add (obj_pong,
                                  "action",
                                  json_object_new_string("pong"));
          ws_sendframe_txt(fd,
                           json_object_to_json_string(obj_pong),
                           2);
          json_object_put(obj_pong);
        }
        else if (strcmp(json_object_get_string (obj_action), "clear") == 0)
        { 
          ws_sendframe_txt(fd,
                           json_object_to_json_string(obj),
                           1);
          char sql[1024];
          sprintf (sql,
                   "INSERT INTO STROKE (time, stroke) VALUES ('%s', '%s')",
                   current_timestamp(),
                   json_object_to_json_string(obj));
          sqlite3_exec (conn,
                        sql,
                        0,
                        0,
                        0);
        }
        else if (strcmp(json_object_get_string (obj_action), "start") == 0)
        {
          recording = true;
          ws_sendframe_txt(fd,
                           json_object_to_json_string(obj),
                           1);
        }
        else if (strcmp(json_object_get_string (obj_action), "stop") == 0)
        { 
          recording = false;
          if (mediafile)
          {
            fclose (mediafile);
            mediafile = NULL;
          }
        }
        else if (strcmp(json_object_get_string (obj_action), "media") == 0)
        { 
          json_object *obj_time, *obj_play;
          json_object_object_get_ex(obj,
                                    "time",
                                     &obj_time);
          obj_play = json_object_new_object();
          json_object_object_add (obj_play,
                                  "fd",
                                  json_object_new_int(fd));
          json_object_object_add (obj_play,
                                  "time",
                                  json_object_get(obj_time));
          // select filename from MEDIA table
          char  sql[1024];
          sprintf (sql,
                   "SELECT filename FROM MEDIA WHERE datetime(time, 'localtime') = '%s'",
                   json_object_get_string(obj_time));
          sqlite3_stmt* stmt;
          sqlite3_prepare_v2 (conn,
                              sql,
                              -1,
                              &stmt,
                              NULL);
          if (sqlite3_step (stmt) == SQLITE_ROW)
          {
            json_object_object_add (obj_play,
                                    "filename",
                                    json_object_new_string((char *)sqlite3_column_text(stmt, 0)));
          }
          sqlite3_finalize (stmt);
  
          // stop current playing thread
          if (playing)
          {
            playing = false;
	    pthread_cancel (pthread_media);
	    pthread_cancel (pthread_stroke);
            pthread_join(pthread_media, NULL);
            pthread_join(pthread_stroke, NULL);
          }
  
          // send start signal to all
          ws_sendframe_txt(fd,
                           "{\"action\": \"start\"}",
                           2);
          playing = true;
  
          // playing the media
          pthread_create(&pthread_media,
                         NULL,
                         playmedia,
                         (void *)json_object_get(obj_play));
          pthread_create(&pthread_stroke,
                         NULL,
                         playstroke,
                         (void *)json_object_get(obj_play));
          json_object_put(obj_play);
        }
        else if (strcmp(json_object_get_string (obj_action), "nick") == 0)
        { 
          json_object *obj_nick;
          json_object_object_get_ex(obj,
                                    "nickname",
                                    &obj_nick);
          char sql[1024];
          sprintf (sql,
                   "UPDATE ONLINE SET nick = '%s' WHERE fd =  %d",
                   json_object_get_string(obj_nick),
                   fd);
          sqlite3_exec (conn,
                        sql,
                        0,
                        0,
                        0);
        }
        else if (strcmp(json_object_get_string (obj_action), "brush") == 0)
        { 
          json_object *obj_brush;
          if (json_object_object_get_ex(obj,
                                        "brush",
                                        &obj_brush))
            mypaint_brush_from_string(brush,
                                      json_object_to_json_string(obj_brush));
        }
        else if (strcmp(json_object_get_string (obj_action), "stroke") == 0)
        {
  
          json_object *obj_width, *obj_height, *obj_stroke;
          if (json_object_object_get_ex(obj,
                                        "width",
                                        &obj_width)
          && json_object_object_get_ex(obj,
                                        "height",
                                        &obj_height)
          && json_object_object_get_ex(obj,
                                        "stroke",
                                        &obj_stroke))
          {
            MyPaintSurface *surface;
            surface = (MyPaintSurface *) mypaint_fixed_tiled_surface_new(json_object_get_int(obj_width),
                                                                         json_object_get_int(obj_height));
            MyPaintTiledSurface *tiled_surface = (MyPaintTiledSurface *)surface;
            mypaint_brush_new_stroke (brush);
            mypaint_brush_reset (brush);
            mypaint_surface_begin_atomic (surface);
            for (size_t i = 0;
                 i <  json_object_array_length(obj_stroke);
                 i++)
            {
              json_object *obj_motion;
              obj_motion = json_object_array_get_idx(obj_stroke,
                                                     i);
              json_object *obj_tmp_tmp;
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
              if (json_object_object_get_ex(obj_motion,
                                            "x",
                                            &obj_tmp_tmp))
                x = json_object_get_double(obj_tmp_tmp);
              if (json_object_object_get_ex(obj_motion,
                                            "y",
                                            &obj_tmp_tmp))
                y = json_object_get_double(obj_tmp_tmp);
              if (json_object_object_get_ex(obj_motion,
                                            "pressure",
                                            &obj_tmp_tmp))
                pressure = json_object_get_double(obj_tmp_tmp);
              if (json_object_object_get_ex(obj_stroke,
                                            "xtilt",
                                            &obj_tmp_tmp))
                xtilt = json_object_get_double(obj_tmp_tmp);
              if (json_object_object_get_ex(obj_stroke,
                                            "ytilt",
                                            &obj_tmp_tmp))
                ytilt = json_object_get_double(obj_tmp_tmp);
              if (json_object_object_get_ex(obj_stroke,
                                            "dtime",
                                            &obj_tmp_tmp))
                dtime = json_object_get_int(obj_tmp_tmp);
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
      
            json_object *new_stroke;
            new_stroke  = json_object_new_array();
            MyPaintRectangle roi;
            MyPaintRectangles rois;
            rois.num_rectangles = 1;
            rois.rectangles = &roi;
            mypaint_surface_end_atomic(surface,
                                       &rois);
            if (roi.width
            && roi.height)
            {
              int tiles_height = ceil(json_object_get_double(obj_height) / MYPAINT_TILE_SIZE);
              int tiles_width = ceil(json_object_get_double(obj_width)/ MYPAINT_TILE_SIZE);
              for (int tx = floor((float)roi.x / MYPAINT_TILE_SIZE); tx < ceil((float)(roi.x + roi.width) / MYPAINT_TILE_SIZE); tx++)
              {
                  for (int ty = floor((float)roi.y / MYPAINT_TILE_SIZE);
                       ty < ceil((float)(roi.y + roi.height) / MYPAINT_TILE_SIZE);
                       ty++)
                  {
                      const int max_x = tx < tiles_width - 1 || json_object_get_int(obj_width) % MYPAINT_TILE_SIZE == 0 ? MYPAINT_TILE_SIZE : json_object_get_int(obj_width) % MYPAINT_TILE_SIZE;
                      const int max_y = ty < tiles_height - 1 || json_object_get_int(obj_height) % MYPAINT_TILE_SIZE == 0 ? MYPAINT_TILE_SIZE : json_object_get_int(obj_height) % MYPAINT_TILE_SIZE;
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
                                  json_object_array_add (new_stroke,
                                                         pixel);
                              }
                          }
                      }
                      mypaint_tiled_surface_tile_request_end(tiled_surface,
                                                             &request);
                  }
              }
              json_object_object_add(obj,
                                     "stroke",
                                      new_stroke);
              ws_sendframe_txt(fd,
                               json_object_to_json_string(obj),
                               2);
              if (recording)
              {
                char *sql = malloc (strlen(json_object_to_json_string(obj)) + 100);
                sprintf (sql,
                         "INSERT INTO STROKE (time, stroke) VALUES ('%s', '%s')",
                         current_timestamp(),
                         json_object_to_json_string(obj));
                sqlite3_exec (conn,
                              sql,
                              0,
                              0,
                              0);
		free (sql);
              }
            }
            mypaint_surface_unref(surface);
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
        if (!mediafile)
        {
          uuid_t binuuid;
          char uuid[37];
          uuid_generate_random(binuuid);
          uuid_unparse_lower(binuuid, uuid);
          char filename[64];
          sprintf (filename,
                   "/tmp/draw/data/media_%s.pcm",
                   uuid);
          mediafile = fopen(filename, "w");
          char sql[1024];
          sprintf (sql,
                   "INSERT INTO MEDIA (time, filename) VALUES ('%s', '%s')",
                   current_timestamp(),
                   filename);
          sqlite3_exec (conn,
                        sql,
                        0,
                        0,
                        0);
        }
        fwrite(msg, 1, size, mediafile);
      }
      break;
    default:
      break;
  }
  free (cli);

}

static void terminate()
{        
  sqlite3_close (conn);
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

  sqlite3_open ("/tmp/draw/data/stroke.db",
                &conn);
  char *sql[] = {"CREATE TABLE IF NOT EXISTS ONLINE (nick, fd, time)",
                "CREATE TABLE IF NOT EXISTS STROKE (time, stroke)",
                "CREATE TABLE IF NOT EXISTS MEDIA (time, filename)",
                "DELETE FROM ONLINE"};
  for (int i = 0; i < 4; i++)
  {
    sqlite3_exec (conn,
                  sql[i],
                  0,
                  0,
                  0);
  }

  recording = playing = false;
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
  signal(SIGABRT, terminate);

  ws_socket(evs, port, 0);
  free (evs);

  return (0);
}
