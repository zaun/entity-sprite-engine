#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "entity/components/entity_component_shape_path.h"
#include "scripting/lua_engine.h"
#include "types/point.h"
#include "types/poly_line.h"
#include "utility/log.h"

// Private helpers
static void _shape_path_skip_separators(const char **s) {
    while (**s && (isspace((unsigned char)**s) || **s == ','))
        (*s)++;
}

static double _shape_path_parse_number(const char **s, int *ok) {
    _shape_path_skip_separators(s);
    char *end = NULL;
    double v = strtod(*s, &end);
    if (end == *s) {
        *ok = 0;
        return 0.0;
    }
    *ok = 1;
    *s = end;
    return v;
}

static void _shape_path_free_lines_and_destroy(EsePolyLine **lines, size_t count) {
    if (!lines)
        return;
    for (size_t i = 0; i < count; ++i) {
        if (lines[i])
            ese_poly_line_destroy(lines[i]);
    }
    free(lines);
}

// Dynamic container helpers
static bool _shape_path_ensure_lines_capacity(EsePolyLine ***lines_ptr, size_t *count,
                                              size_t *cap) {
    if (*count < *cap)
        return true;
    size_t new_cap = *cap ? (*cap * 2) : 4;
    EsePolyLine **tmp = (EsePolyLine **)realloc(*lines_ptr, sizeof(EsePolyLine *) * new_cap);
    if (!tmp)
        return false;
    *lines_ptr = tmp;
    *cap = new_cap;
    return true;
}

// Command handlers return true on success, false on error.
// They may create/append to 'current' and update positions.

// moveto: starts new subpath; also handles implicit following lineto pairs
static bool _shape_path_handle_moveto(const EseLuaEngine *engine, float scale, const char **pp,
                                      EsePolyLine ***lines_ptr, size_t *lines_count,
                                      size_t *lines_cap, EsePolyLine **pcurrent, float *cx,
                                      float *cy, float *spx, float *spy) {
    int ok;
    double x = _shape_path_parse_number(pp, &ok);
    if (!ok)
        return false;
    double y = _shape_path_parse_number(pp, &ok);
    if (!ok)
        return false;

    if (!_shape_path_ensure_lines_capacity(lines_ptr, lines_count, lines_cap))
        return false;

    EsePolyLine *pl = ese_poly_line_create((EseLuaEngine *)engine);
    if (!pl)
        return false;

    (*lines_ptr)[(*lines_count)++] = pl;
    *pcurrent = pl;

    if (isfinite(x) && isfinite(y)) {
        *cx = (float)(x * scale);
        *cy = (float)(y * scale);
    } else {
        *cx = 0.0f;
        *cy = 0.0f;
    }

    *spx = *cx;
    *spy = *cy;

    EsePoint *pt = ese_point_create((EseLuaEngine *)engine);
    if (!pt)
        return false;
    ese_point_set_x(pt, *cx);
    ese_point_set_y(pt, *cy);
    if (!ese_poly_line_add_point(pl, pt))
        return false;

    _shape_path_skip_separators(pp);
    while (**pp && (isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.')) {
        double nx = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double ny = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        *cx = (float)(nx * scale);
        *cy = (float)(ny * scale);
        EsePoint *pt2 = ese_point_create((EseLuaEngine *)engine);
        if (!pt2)
            return false;
        ese_point_set_x(pt2, *cx);
        ese_point_set_y(pt2, *cy);
        if (!ese_poly_line_add_point(pl, pt2))
            return false;
        _shape_path_skip_separators(pp);
    }
    return true;
}

// lineto: one or more coordinate pairs
static bool _shape_path_handle_lineto(const EseLuaEngine *engine, float scale, const char **pp,
                                      EsePolyLine **pcurrent, float *cx, float *cy, bool relative,
                                      EsePolyLine ***lines_ptr, size_t *lines_count,
                                      size_t *lines_cap) {
    while (1) {
        _shape_path_skip_separators(pp);
        if (!**pp || !(isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.'))
            break;
        int ok;
        double x = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double y = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        if (relative) {
            x += (*cx / scale);
            y += (*cy / scale);
        }
        *cx = (float)(x * scale);
        *cy = (float)(y * scale);

        if (!*pcurrent) {
            if (!_shape_path_ensure_lines_capacity(lines_ptr, lines_count, lines_cap))
                return false;
            EsePolyLine *pl = ese_poly_line_create((EseLuaEngine *)engine);
            if (!pl)
                return false;
            (*lines_ptr)[(*lines_count)++] = pl;
            *pcurrent = pl;
        }

        EsePoint *pt = ese_point_create((EseLuaEngine *)engine);
        if (!pt)
            return false;
        ese_point_set_x(pt, *cx);
        ese_point_set_y(pt, *cy);
        if (!ese_poly_line_add_point(*pcurrent, pt))
            return false;
    }
    return true;
}

// horizontal lineto: one or more x values
static bool _shape_path_handle_hlineto(const EseLuaEngine *engine, float scale, const char **pp,
                                       EsePolyLine **pcurrent, float *cx, float *cy, bool relative,
                                       EsePolyLine ***lines_ptr, size_t *lines_count,
                                       size_t *lines_cap) {
    while (1) {
        _shape_path_skip_separators(pp);
        if (!**pp || !(isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.'))
            break;
        int ok;
        double x = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        if (relative)
            x += (*cx / scale);
        *cx = (float)(x * scale);

        if (!*pcurrent) {
            if (!_shape_path_ensure_lines_capacity(lines_ptr, lines_count, lines_cap))
                return false;
            EsePolyLine *pl = ese_poly_line_create((EseLuaEngine *)engine);
            if (!pl)
                return false;
            (*lines_ptr)[(*lines_count)++] = pl;
            *pcurrent = pl;
        }

        EsePoint *pt = ese_point_create((EseLuaEngine *)engine);
        if (!pt)
            return false;
        ese_point_set_x(pt, *cx);
        ese_point_set_y(pt, *cy);
        if (!ese_poly_line_add_point(*pcurrent, pt))
            return false;
    }
    return true;
}

// vertical lineto: one or more y values
static bool _shape_path_handle_vlineto(const EseLuaEngine *engine, float scale, const char **pp,
                                       EsePolyLine **pcurrent, float *cx, float *cy, bool relative,
                                       EsePolyLine ***lines_ptr, size_t *lines_count,
                                       size_t *lines_cap) {
    while (1) {
        _shape_path_skip_separators(pp);
        if (!**pp || !(isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.'))
            break;
        int ok;
        double y = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        if (relative)
            y += (*cy / scale);
        *cy = (float)(y * scale);

        if (!*pcurrent) {
            if (!_shape_path_ensure_lines_capacity(lines_ptr, lines_count, lines_cap))
                return false;
            EsePolyLine *pl = ese_poly_line_create((EseLuaEngine *)engine);
            if (!pl)
                return false;
            (*lines_ptr)[(*lines_count)++] = pl;
            *pcurrent = pl;
        }

        EsePoint *pt = ese_point_create((EseLuaEngine *)engine);
        if (!pt)
            return false;
        ese_point_set_x(pt, *cx);
        ese_point_set_y(pt, *cy);
        if (!ese_poly_line_add_point(*pcurrent, pt))
            return false;
    }
    return true;
}

// cubic bezier:
static bool _shape_path_handle_cubic_bezier(const EseLuaEngine *engine, float scale,
                                            const char **pp, EsePolyLine **pcurrent, float *cx,
                                            float *cy, bool relative, EsePolyLine ***lines_ptr,
                                            size_t *lines_count, size_t *lines_cap,
                                            float *prev_ctrl_x, float *prev_ctrl_y,
                                            bool *prev_ctrl_valid) {
    // Parse sets of: x1,y1 x2,y2 x,y and approximate with line segments
    const int segments = 16;
    while (1) {
        _shape_path_skip_separators(pp);
        if (!**pp || !(isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.'))
            break;

        int ok;
        double x1 = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double y1 = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double x2 = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double y2 = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double x = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double y = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;

        if (relative) {
            x1 += (*cx / scale);
            y1 += (*cy / scale);
            x2 += (*cx / scale);
            y2 += (*cy / scale);
            x += (*cx / scale);
            y += (*cy / scale);
        }

        float x0 = *cx, y0 = *cy;
        float cx1 = (float)(x1 * scale), cy1 = (float)(y1 * scale);
        float cx2 = (float)(x2 * scale), cy2 = (float)(y2 * scale);
        float x3 = (float)(x * scale), y3 = (float)(y * scale);

        if (!*pcurrent) {
            if (!_shape_path_ensure_lines_capacity(lines_ptr, lines_count, lines_cap))
                return false;
            EsePolyLine *pl = ese_poly_line_create((EseLuaEngine *)engine);
            if (!pl)
                return false;
            (*lines_ptr)[(*lines_count)++] = pl;
            *pcurrent = pl;
            // If there was no starting point, seed it with current position
            EsePoint *pt0 = ese_point_create((EseLuaEngine *)engine);
            if (!pt0)
                return false;
            ese_point_set_x(pt0, x0);
            ese_point_set_y(pt0, y0);
            if (!ese_poly_line_add_point(*pcurrent, pt0))
                return false;
        }

        for (int i = 1; i <= segments; ++i) {
            float t = (float)i / (float)segments;
            float mt = 1.0f - t;
            float bx =
                mt * mt * mt * x0 + 3 * mt * mt * t * cx1 + 3 * mt * t * t * cx2 + t * t * t * x3;
            float by =
                mt * mt * mt * y0 + 3 * mt * mt * t * cy1 + 3 * mt * t * t * cy2 + t * t * t * y3;
            EsePoint *pt = ese_point_create((EseLuaEngine *)engine);
            if (!pt)
                return false;
            ese_point_set_x(pt, bx);
            ese_point_set_y(pt, by);
            if (!ese_poly_line_add_point(*pcurrent, pt))
                return false;
        }

        // Update current position and previous control
        *cx = x3;
        *cy = y3;
        *prev_ctrl_x = cx2;
        *prev_ctrl_y = cy2;
        *prev_ctrl_valid = true;
    }
    return true;
}

// smooth cubic bezier:
static bool _shape_path_handle_smooth_cubic_bezier(const EseLuaEngine *engine, float scale,
                                                   const char **pp, EsePolyLine **pcurrent,
                                                   float *cx, float *cy, bool relative,
                                                   EsePolyLine ***lines_ptr, size_t *lines_count,
                                                   size_t *lines_cap, float *prev_ctrl_x,
                                                   float *prev_ctrl_y, bool *prev_ctrl_valid) {
    // Parse sets of: x2,y2 x,y with first control reflected from previous
    const int segments = 16;
    while (1) {
        _shape_path_skip_separators(pp);
        if (!**pp || !(isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.'))
            break;

        int ok;
        double x2 = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double y2 = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double x = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double y = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;

        float x0 = *cx, y0 = *cy;
        float cx1, cy1;
        if (*prev_ctrl_valid) {
            cx1 = 2.0f * x0 - *prev_ctrl_x;
            cy1 = 2.0f * y0 - *prev_ctrl_y;
        } else {
            cx1 = x0;
            cy1 = y0;
        }

        if (relative) {
            x2 += (*cx / scale);
            y2 += (*cy / scale);
            x += (*cx / scale);
            y += (*cy / scale);
        }

        float cx2 = (float)(x2 * scale), cy2 = (float)(y2 * scale);
        float x3 = (float)(x * scale), y3 = (float)(y * scale);

        if (!*pcurrent) {
            if (!_shape_path_ensure_lines_capacity(lines_ptr, lines_count, lines_cap))
                return false;
            EsePolyLine *pl = ese_poly_line_create((EseLuaEngine *)engine);
            if (!pl)
                return false;
            (*lines_ptr)[(*lines_count)++] = pl;
            *pcurrent = pl;
            // Seed with current position
            EsePoint *pt0 = ese_point_create((EseLuaEngine *)engine);
            if (!pt0)
                return false;
            ese_point_set_x(pt0, x0);
            ese_point_set_y(pt0, y0);
            if (!ese_poly_line_add_point(*pcurrent, pt0))
                return false;
        }

        for (int i = 1; i <= segments; ++i) {
            float t = (float)i / (float)segments;
            float mt = 1.0f - t;
            float bx =
                mt * mt * mt * x0 + 3 * mt * mt * t * cx1 + 3 * mt * t * t * cx2 + t * t * t * x3;
            float by =
                mt * mt * mt * y0 + 3 * mt * mt * t * cy1 + 3 * mt * t * t * cy2 + t * t * t * y3;
            EsePoint *pt = ese_point_create((EseLuaEngine *)engine);
            if (!pt)
                return false;
            ese_point_set_x(pt, bx);
            ese_point_set_y(pt, by);
            if (!ese_poly_line_add_point(*pcurrent, pt))
                return false;
        }

        *cx = x3;
        *cy = y3;
        *prev_ctrl_x = cx2;
        *prev_ctrl_y = cy2;
        *prev_ctrl_valid = true;
    }
    return true;
}

// quadratic bezier:
static bool _shape_path_handle_quadratic_bezier(const EseLuaEngine *engine, float scale,
                                                const char **pp, EsePolyLine **pcurrent, float *cx,
                                                float *cy, bool relative, EsePolyLine ***lines_ptr,
                                                size_t *lines_count, size_t *lines_cap) {
    (void)engine;
    (void)pcurrent;
    (void)cx;
    (void)cy;
    (void)relative;
    (void)lines_ptr;
    (void)lines_count;
    (void)lines_cap;
    (void)scale;
    _shape_path_skip_separators(pp);
    while (**pp && !isalpha((unsigned char)**pp)) {
        ++(*pp);
    }
    return true;
}

// smooth quadratic bezier:
static bool _shape_path_handle_smooth_quadratic_bezier(const EseLuaEngine *engine, float scale,
                                                       const char **pp, EsePolyLine **pcurrent,
                                                       float *cx, float *cy, bool relative,
                                                       EsePolyLine ***lines_ptr,
                                                       size_t *lines_count, size_t *lines_cap) {
    (void)engine;
    (void)pcurrent;
    (void)cx;
    (void)cy;
    (void)relative;
    (void)lines_ptr;
    (void)lines_count;
    (void)lines_cap;
    (void)scale;
    _shape_path_skip_separators(pp);
    while (**pp && !isalpha((unsigned char)**pp)) {
        ++(*pp);
    }
    return true;
}

// arc curve:
static bool _shape_path_handle_arc(EseLuaEngine *engine, float scale, const char **pp,
                                   EsePolyLine **pcurrent, float *cx, float *cy, bool relative,
                                   EsePolyLine ***lines_ptr, size_t *lines_count,
                                   size_t *lines_cap) {
    // Parse sets of: rx,ry rotation large-arc-flag sweep-flag x,y
    const int segments = 16;
    while (1) {
        _shape_path_skip_separators(pp);
        if (!**pp || !(isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.'))
            break;

        int ok;
        double rx = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double ry = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double rotation = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double large_arc = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double sweep = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double x = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;
        double y = _shape_path_parse_number(pp, &ok);
        if (!ok)
            break;

        if (relative) {
            x += (*cx / scale);
            y += (*cy / scale);
        }

        float x0 = *cx, y0 = *cy;
        float x1 = (float)(x * scale), y1 = (float)(y * scale);
        float rx_scaled = (float)(rx * scale), ry_scaled = (float)(ry * scale);

        if (!*pcurrent) {
            if (!_shape_path_ensure_lines_capacity(lines_ptr, lines_count, lines_cap))
                return false;
            EsePolyLine *pl = ese_poly_line_create(engine);
            if (!pl)
                return false;
            (*lines_ptr)[(*lines_count)++] = pl;
            *pcurrent = pl;
        }

        // Create a proper circle using the arc parameters
        // For a full circle, we need to create two semicircles
        float center_x = (x0 + x1) / 2.0f;
        float center_y = (y0 + y1) / 2.0f;

        // First semicircle (top half)
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)(M_PI * i / segments);
            float px = center_x + rx_scaled * cosf(angle);
            float py = center_y + ry_scaled * sinf(angle);

            EsePoint *pt = ese_point_create(engine);
            if (!pt)
                return false;
            ese_point_set_x(pt, px);
            ese_point_set_y(pt, py);
            if (!ese_poly_line_add_point(*pcurrent, pt))
                return false;
        }

        // Second semicircle (bottom half)
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)(M_PI + M_PI * i / segments);
            float px = center_x + rx_scaled * cosf(angle);
            float py = center_y + ry_scaled * sinf(angle);

            EsePoint *pt = ese_point_create(engine);
            if (!pt)
                return false;
            ese_point_set_x(pt, px);
            ese_point_set_y(pt, py);
            if (!ese_poly_line_add_point(*pcurrent, pt))
                return false;
        }

        *cx = x1;
        *cy = y1;
        _shape_path_skip_separators(pp);
        while (*pp && (isdigit((unsigned char)**pp) || **pp == '+' || **pp == '-' || **pp == '.' ||
                       **pp == ',' || isspace((unsigned char)**pp))) {
            ++(*pp);
        }
    }
    return true;
}

// closepath: add subpath start if needed and mark CLOSED
static bool _shape_path_handle_closepath(const EseLuaEngine *engine, EsePolyLine **pcurrent,
                                         float *cx, float *cy, float spx, float spy) {
    if (!*pcurrent)
        return true;
    size_t pc = ese_poly_line_get_point_count(*pcurrent);
    if (pc > 0) {
        const float *pts = ese_poly_line_get_points(*pcurrent);
        float last_x = pts[(pc - 1) * 2];
        float last_y = pts[(pc - 1) * 2 + 1];
        if (last_x != spx || last_y != spy) {
            EsePoint *pt = ese_point_create((EseLuaEngine *)engine);
            if (!pt)
                return false;
            ese_point_set_x(pt, spx);
            ese_point_set_y(pt, spy);
            if (!ese_poly_line_add_point(*pcurrent, pt))
                return false;
        }
    }
    ese_poly_line_set_type(*pcurrent, POLY_LINE_CLOSED);
    // SVG spec: after 'Z' the current point becomes the subpath’s initial point
    // so subsequent relative commands use (spx, spy) as origin.
    if (cx)
        *cx = spx;
    if (cy)
        *cy = spy;
    return true;
}

// ----------------------------------------
// Public functions
EsePolyLine **shape_path_to_polylines(EseLuaEngine *engine, float scale, const char *path,
                                      size_t *out_count) {
    if (out_count)
        *out_count = 0;
    if (!engine || !path)
        return NULL;

    const char *p = path;
    EsePolyLine **lines = NULL;
    size_t lines_count = 0;
    size_t lines_cap = 0;

    EsePolyLine *current = NULL;
    float cx = 0.0f;
    float cy = 0.0f;
    float spx = 0.0f;
    float spy = 0.0f;
    char cmd = 0;

    // Track previous cubic control point for smooth curves
    float prev_ctrl_x = 0.0f, prev_ctrl_y = 0.0f;
    bool prev_ctrl_valid = false;

    while (*p) {
        _shape_path_skip_separators(&p);
        if (!*p)
            break;

        if (isalpha((unsigned char)*p)) {
            cmd = *p++;
            _shape_path_skip_separators(&p);
        } else if (cmd == 0) {
            log_debug("SVG", "shape_path_to_polylines: path did not start with a command");
            _shape_path_free_lines_and_destroy(lines, lines_count);
            return NULL;
        }

        bool ok = true;
        switch (cmd) {
        case 'M': // Move To
            ok = _shape_path_handle_moveto(engine, scale, &p, &lines, &lines_count, &lines_cap,
                                           &current, &cx, &cy, &spx, &spy);
            prev_ctrl_valid = false;
            break;
        case 'm': { // Relative Move To
            /* relative moveto: parse then offset current pos */
            int parse_ok;
            double rx = _shape_path_parse_number(&p, &parse_ok);
            if (!parse_ok) {
                ok = false;
                break;
            }
            double ry = _shape_path_parse_number(&p, &parse_ok);
            if (!parse_ok) {
                ok = false;
                break;
            }
            rx = rx + (cx / scale);
            ry = ry + (cy / scale);
            char save_cmd = cmd;
            /* reuse absolute moveto handler by prepping string pointer back by
             * preventing extra behavior */
            const char *saved_p = p;
            p = saved_p; /* no change; we already consumed numbers */
            /* create the polyline similarly to absolute moveto behavior */
            if (!_shape_path_ensure_lines_capacity(&lines, &lines_count, &lines_cap)) {
                ok = false;
                break;
            }
            EsePolyLine *pl = ese_poly_line_create(engine);
            if (!pl) {
                ok = false;
                break;
            }
            lines[lines_count++] = pl;
            current = pl;
            cx = (float)(rx * scale);
            cy = (float)(ry * scale);
            spx = cx;
            spy = cy;
            EsePoint *pt = ese_point_create(engine);
            if (!pt) {
                ok = false;
                break;
            }
            ese_point_set_x(pt, cx);
            ese_point_set_y(pt, cy);
            if (!ese_poly_line_add_point(pl, pt)) {
                ok = false;
                break;
            }
            _shape_path_skip_separators(&p);
            while (*p && (isdigit((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.')) {
                int ok2;
                double nx = _shape_path_parse_number(&p, &ok2);
                if (!ok2)
                    break;
                double ny = _shape_path_parse_number(&p, &ok2);
                if (!ok2)
                    break;
                nx = nx + (cx / scale);
                ny = ny + (cy / scale); /* relative to previous current */
                cx = (float)(nx * scale);
                cy = (float)(ny * scale);
                EsePoint *pt2 = ese_point_create(engine);
                if (!pt2) {
                    ok = false;
                    break;
                }
                ese_point_set_x(pt2, cx);
                ese_point_set_y(pt2, cy);
                if (!ese_poly_line_add_point(current, pt2)) {
                    ok = false;
                    break;
                }
            }
            prev_ctrl_valid = false;
            break;
        }
        case 'L': // Line To
            ok = _shape_path_handle_lineto(engine, scale, &p, &current, &cx, &cy, false, &lines,
                                           &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'l': // Relative Line To
            ok = _shape_path_handle_lineto(engine, scale, &p, &current, &cx, &cy, true, &lines,
                                           &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'H': // Horizontal Line
            ok = _shape_path_handle_hlineto(engine, scale, &p, &current, &cx, &cy, false, &lines,
                                            &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'h':
            ok = _shape_path_handle_hlineto(engine, scale, &p, &current, &cx, &cy, true, &lines,
                                            &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'V': // Vertical Line
            ok = _shape_path_handle_vlineto(engine, scale, &p, &current, &cx, &cy, false, &lines,
                                            &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'v': // Relative Vertical Line
            ok = _shape_path_handle_vlineto(engine, scale, &p, &current, &cx, &cy, true, &lines,
                                            &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'C': // Cubic Bezier Curve
            ok = _shape_path_handle_cubic_bezier(engine, scale, &p, &current, &cx, &cy, false,
                                                 &lines, &lines_count, &lines_cap, &prev_ctrl_x,
                                                 &prev_ctrl_y, &prev_ctrl_valid);
            break;
        case 'c': // Relative Cubic Bezier Curve
            ok = _shape_path_handle_cubic_bezier(engine, scale, &p, &current, &cx, &cy, true,
                                                 &lines, &lines_count, &lines_cap, &prev_ctrl_x,
                                                 &prev_ctrl_y, &prev_ctrl_valid);
            break;
        case 'S': // Smooth Cubic Bezier Curve
            ok = _shape_path_handle_smooth_cubic_bezier(
                engine, scale, &p, &current, &cx, &cy, false, &lines, &lines_count, &lines_cap,
                &prev_ctrl_x, &prev_ctrl_y, &prev_ctrl_valid);
            break;
        case 's': // Relative Smooth Cubic Bezier Curve
            ok = _shape_path_handle_smooth_cubic_bezier(
                engine, scale, &p, &current, &cx, &cy, true, &lines, &lines_count, &lines_cap,
                &prev_ctrl_x, &prev_ctrl_y, &prev_ctrl_valid);
            break;
        case 'Q': // Quadratic Bezier Curve
            ok = _shape_path_handle_quadratic_bezier(engine, scale, &p, &current, &cx, &cy, false,
                                                     &lines, &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'q': // Relative Quadratic Bezier Curve
            ok = _shape_path_handle_quadratic_bezier(engine, scale, &p, &current, &cx, &cy, true,
                                                     &lines, &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'T': // Smooth Quadratic Bezier Curve
            ok = _shape_path_handle_smooth_quadratic_bezier(
                engine, scale, &p, &current, &cx, &cy, false, &lines, &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 't': // Relative Smooth Quadratic Bezier Curve
            ok = _shape_path_handle_smooth_quadratic_bezier(engine, scale, &p, &current, &cx, &cy,
                                                            true, &lines, &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'A': // Arc Curve
            ok = _shape_path_handle_arc(engine, scale, &p, &current, &cx, &cy, false, &lines,
                                        &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'a': // Relative Arc Curve
            ok = _shape_path_handle_arc(engine, scale, &p, &current, &cx, &cy, true, &lines,
                                        &lines_count, &lines_cap);
            prev_ctrl_valid = false;
            break;
        case 'Z': // Close Path
        case 'z':
            ok = _shape_path_handle_closepath(engine, &current, &cx, &cy, spx, spy);
            prev_ctrl_valid = false;
            _shape_path_skip_separators(&p);
            break;
        default:
            log_debug("SVG",
                      "Unsupported SVG command '%c' encountered; skipping its "
                      "parameters",
                      cmd);
            while (*p && !isalpha((unsigned char)*p))
                p++;
            break;
        }
        if (!ok) {
            _shape_path_free_lines_and_destroy(lines, lines_count);
            return NULL;
        }
    }

    if (out_count)
        *out_count = lines_count;
    return lines;
}
