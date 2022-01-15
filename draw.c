// Compile with: gcc hello_fastcgi.c -o hello_fastcgi.fcgi -lfcgi -O3 -Wall -Wextra -pedantic -std=c11
#include <fcgi_stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mypaint-brush.h>
#include <mypaint-fixed-tiled-surface.h>
#include <json-c/json.h>
#include <math.h>
#include "brush.h"

int main (void) {
    /*
        Initialization
    */
    MyPaintBrush *brush = mypaint_brush_new();
    mypaint_brush_from_string(brush, brush_str);
    char buf[1024];
    char *p;

    while (FCGI_Accept() >= 0) {
        puts("Status: 200 OK\r\n"
             "Content-type: application/json\r\n\r\n");

	int len = 0;
	p = getenv("CONTENT_LENGTH");
	if (p)
	{
            len = atoi(p);
	    sprintf (buf, "get content length: %d\n", len);
	    FCGI_perror (buf);
	}
	if (!len)
            continue;
        char *str = malloc(len);
        fread(str, len, 1, stdin);
        json_object *obj = json_tokener_parse(str);
        free(str);
        int width;
        int height;
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
            float x, y, pressure, xtilt, ytilt, dtime;
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
            mypaint_brush_stroke_to (brush,
                                     surface,
                                     x,
                                     y,
                                     pressure,
                                     xtilt,
                                     ytilt,
                                     dtime);
        }
        MyPaintRectangle roi;
        mypaint_surface_end_atomic(surface, &roi);

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
                        const int yy = ty * tile_size_pixels + y;
                        for (int x = 0; x < max_x; x++)
                        {
                            int xx = tx * tile_size_pixels + x;
                            int yy = ty * tile_size_pixels + y;
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

        puts(json_object_to_json_string_ext (out, JSON_C_TO_STRING_PLAIN));
    }
    return EXIT_SUCCESS;
}
