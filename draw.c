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
#include <unistd.h>
#include <string.h>
#include <mypaint-brush.h>
#include <mypaint-fixed-tiled-surface.h>
#include <json-c/json.h>
#include <math.h>
#include <ws.h>

#define BRUSH_FILE "/usr/share/mypaint/brushes/classic/kabura.myb"
//#define BRUSH_FILE "/usr/share/mypaint-data/1.0/brushes/classic/kabura.myb"
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
    else if (size == 1
    && msg[0] == 0x09)
    {
#ifndef DISABLE_VERBOSE
      printf("I receive a ping message.\n");
#endif
      char pong = 0xa;
      ws_sendframe(fd, &pong, 1, true, type);
      return;
    }

    /**
     * Mimicks the same frame type received and re-send it again
     *
     * Please note that we could just use a ws_sendframe_txt()
     * or ws_sendframe_bin() here, but we're just being safe
     * and re-sending the very same frame type and content
     * again.
     */
    json_object *obj = json_tokener_parse((char *)msg);
    int width, height;
    struct array_list *motions;

    json_object *obj_tmp;
    json_object_object_get_ex(obj, "width", &obj_tmp);
    width = json_object_get_int(obj_tmp);
    json_object_object_get_ex(obj, "height", &obj_tmp);
    height = json_object_get_int(obj_tmp);
    json_object_object_get_ex(obj, "stroke", &obj_tmp);
    motions = json_object_get_array (obj_tmp);
    MyPaintSurface *surface = (MyPaintSurface *) mypaint_fixed_tiled_surface_new(width, height);
    MyPaintTiledSurface *tiled_surface = (MyPaintTiledSurface *)surface;
    mypaint_brush_reset (brush);
    mypaint_brush_new_stroke (brush);
    mypaint_surface_begin_atomic (surface);
    for (int i = 0; i <  array_list_length(motions); i++)
    {
        json_object *motion = (json_object *)array_list_get_idx(motions, i);
        float x, y, pressure, xtilt, ytilt, dtime, viewzoom, viewrotation, barrel_rotation;
	int linear;
        json_object_object_get_ex(motion, "x", &obj_tmp);
        x = json_object_get_double(obj_tmp);
        json_object_object_get_ex(motion, "y", &obj_tmp);
        y = json_object_get_double(obj_tmp);
        json_object_object_get_ex(motion, "pressure", &obj_tmp);
        pressure = json_object_get_double(obj_tmp);
        json_object_object_get_ex(motion, "xtilt", &obj_tmp);
        xtilt = json_object_get_double(obj_tmp);
        json_object_object_get_ex(motion, "ytilt", &obj_tmp);
        ytilt = json_object_get_double(obj_tmp);
        json_object_object_get_ex(motion, "dtime", &obj_tmp);
        dtime = (float)json_object_get_double(obj_tmp) / 1000;
	viewzoom = 1.0;
	viewrotation = 0;
	barrel_rotation = 0;
	linear = 0;
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
    mypaint_surface_end_atomic(surface, &rois);

    json_object *out = json_object_new_array();
    if (roi.width
    && roi.height)
    {
        int tile_size_pixels = MYPAINT_TILE_SIZE;
        int tiles_height = ceil((float)height / tile_size_pixels);
        int tiles_width = ceil((float)width / tile_size_pixels);
        for (int tx = floor((float)roi.x / tile_size_pixels); tx < ceil((float)(roi.x + roi.width) / tile_size_pixels); tx++)
        {
            for (int ty = floor((float)roi.y / tile_size_pixels); ty < ceil((float)(roi.y + roi.height) / tile_size_pixels); ty++)
            {
                const int max_x = tx < tiles_width - 1 || width % tile_size_pixels == 0 ? tile_size_pixels : width % tile_size_pixels;
                const int max_y = ty < tiles_height - 1 || height % tile_size_pixels == 0 ? tile_size_pixels : height % tile_size_pixels;
                MyPaintTileRequest request;
                mypaint_tile_request_init(&request, 0, tx, ty, TRUE);
                mypaint_tiled_surface_tile_request_start(tiled_surface, &request);
                for (int y = 0; y < max_y; y++)
                {
                    int yy = ty * tile_size_pixels + y;
                    for (int x = 0; x < max_x; x++)
                    {
                        int xx = tx * tile_size_pixels + x;
                        int offset = tile_size_pixels * y + x;
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
                            json_object_array_add (pixel, json_object_new_int(xx));
                            json_object_array_add (pixel, json_object_new_int(yy));
                            json_object_array_add (pixel, json_object_new_int(r));
                            json_object_array_add (pixel, json_object_new_int(g));
                            json_object_array_add (pixel, json_object_new_int(b));
                            json_object_array_add (out, pixel);
                        }
                    }
                }
                mypaint_tiled_surface_tile_request_end(tiled_surface, &request);
            }
        }
    }
    mypaint_tiled_surface_destroy (tiled_surface);

    const char *p = json_object_to_json_string_ext (out, JSON_C_TO_STRING_PLAIN);
    ws_sendframe(fd, p, strlen(p), true, type);
}

/**
 * @brief Main routine.
 *
 * @note After invoking @ref ws_socket, this routine never returns,
 * unless if invoked from a different thread.
 */
int main(void)
{
    /*
        Initialize the brush
    */
    brush = mypaint_brush_new();
    FILE *fp = fopen (BRUSH_FILE, "r");
    fseek (fp, 0, SEEK_END);
    int size = ftell (fp);
    rewind (fp);

    char *buf = (char *)malloc (size);
    fread (buf, 1, size, fp);
    fclose (fp);
    mypaint_brush_from_string(brush, buf);
    free (buf);

    struct ws_events evs;
    evs.onopen    = &onopen;
    evs.onclose   = &onclose;
    evs.onmessage = &onmessage;
    if (fork() == 0)
        ws_socket(&evs, 8080, 0); /* Never returns. */

    return (0);
}
