//-----------------------------------------------------------------------------
// Helper functions for the text-based browser window.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "obj/icons-proto.h"
#include <stdarg.h>

const TextWindow::Color TextWindow::fgColors[] = {
    { 'd', RGB(255, 255, 255) },
    { 'l', RGB(100, 100, 255) },
    { 't', RGB(255, 200,   0) },
    { 'h', RGB( 90,  90,  90) },
    { 's', RGB( 40, 255,  40) },
    { 'm', RGB(200, 200,   0) },
    { 'r', RGB(  0,   0,   0) },
    { 'x', RGB(255,  20,  20) },
    { 'i', RGB(  0, 255, 255) },
    { 'g', RGB(160, 160, 160) },
    { 'b', RGB(200, 200, 200) },
    { 0, 0 },
};
const TextWindow::Color TextWindow::bgColors[] = {
    { 'd', RGB(  0,   0,   0) },
    { 't', RGB( 34,  15,  15) },
    { 'a', RGB( 25,  25,  25) },
    { 'r', RGB(255, 255, 255) },
    { 0, 0 },
};

bool TextWindow::SPACER = false;
TextWindow::HideShowIcon TextWindow::hideShowIcons[] = {
    { &(SS.GW.showWorkplanes),  Icon_workplane,     "workplanes from inactive groups"},
    { &(SS.GW.showNormals),     Icon_normal,        "normals"                        },
    { &(SS.GW.showPoints),      Icon_point,         "points"                         },
    { &(SS.GW.showConstraints), Icon_constraint,    "constraints and dimensions"     },
    { &(SS.GW.showFaces),       Icon_faces,         "XXX - special cased"            },
    { &SPACER, 0 },
    { &(SS.GW.showShaded),      Icon_shaded,        "shaded view of solid model"     },
    { &(SS.GW.showEdges),       Icon_edges,         "edges of solid model"           },
    { &(SS.GW.showMesh),        Icon_mesh,          "triangle mesh of solid model"   },
    { &SPACER, 0 },
    { &(SS.GW.showHdnLines),    Icon_hidden_lines,  "hidden lines"                   },
    { 0, 0 },
};

void TextWindow::MakeColorTable(const Color *in, float *out) {
    int i;
    for(i = 0; in[i].c != 0; i++) {
        int c = in[i].c;
        if(c < 0 || c > 255) oops();
        out[c*3 + 0] = REDf(in[i].color);
        out[c*3 + 1] = GREENf(in[i].color);
        out[c*3 + 2] = BLUEf(in[i].color);
    }
}

void TextWindow::Init(void) {
    ClearSuper();
}

void TextWindow::ClearSuper(void) {
    HideEditControl();

    memset(this, 0, sizeof(*this));
    MakeColorTable(fgColors, fgColorTable);
    MakeColorTable(bgColors, bgColorTable);

    ClearScreen();
    Show();
}

void TextWindow::HideEditControl(void) {
    editControl.colorPicker.show = false;
    HideTextEditControl();
}

void TextWindow::ShowEditControl(int halfRow, int col, char *s) {
    editControl.halfRow = halfRow;
    editControl.col = col;

    int x = LEFT_MARGIN + CHAR_WIDTH*col;
    int y = (halfRow - SS.TW.scrollPos)*(LINE_HEIGHT/2);

    ShowTextEditControl(x - 3, y + 2, s);
}

void TextWindow::ShowEditControlWithColorPicker(int halfRow, int col, DWORD rgb)
{
    char str[1024];
    sprintf(str, "%.2f, %.2f, %.2f", REDf(rgb), GREENf(rgb), BLUEf(rgb));

    SS.later.showTW = true;

    editControl.colorPicker.show = true;
    editControl.colorPicker.rgb = rgb;
    editControl.colorPicker.h = 0;
    editControl.colorPicker.s = 0;
    editControl.colorPicker.v = 1;
    ShowEditControl(halfRow, col, str);
}

void TextWindow::ClearScreen(void) {
    int i, j;
    for(i = 0; i < MAX_ROWS; i++) {
        for(j = 0; j < MAX_COLS; j++) {
            text[i][j] = ' ';
            meta[i][j].fg = 'd';
            meta[i][j].bg = 'd';
            meta[i][j].link = NOT_A_LINK;
        }
        top[i] = i*2;
    }
    rows = 0;
}

void TextWindow::Printf(bool halfLine, char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    if(rows >= MAX_ROWS) return;

    int r, c;
    r = rows;
    top[r] = (r == 0) ? 0 : (top[r-1] + (halfLine ? 3 : 2));
    rows++;

    for(c = 0; c < MAX_COLS; c++) {
        text[r][c] = ' ';
        meta[r][c].link = NOT_A_LINK;
    }

    int fg = 'd', bg = 'd';
    int link = NOT_A_LINK;
    DWORD data = 0;
    LinkFunction *f = NULL, *h = NULL;

    c = 0;
    while(*fmt) {
        char buf[1024];

        if(*fmt == '%') {
            fmt++;
            if(*fmt == '\0') goto done;
            strcpy(buf, "");
            switch(*fmt) {
                case 'd': {
                    int v = va_arg(vl, int);
                    sprintf(buf, "%d", v);
                    break;
                }
                case 'x': {
                    DWORD v = va_arg(vl, DWORD);
                    sprintf(buf, "%08x", v);
                    break;
                }
                case '@': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%.2f", v);
                    break;
                }
                case '2': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.2f", v < 0 ? "" : " ", v);
                    break;
                }
                case '3': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.3f", v < 0 ? "" : " ", v);
                    break;
                }
                case '#': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%.3f", v);
                    break;
                }
                case 's': {
                    char *s = va_arg(vl, char *);
                    memcpy(buf, s, min(sizeof(buf), strlen(s)+1));
                    break;
                }
                case 'c': {
                    char v = va_arg(vl, char);
                    if(v == 0) {
                        strcpy(buf, "");
                    } else {
                        sprintf(buf, "%c", v);
                    }
                    break;
                }
                case 'E':
                    fg = 'd';
                    // leave the background, though
                    link = NOT_A_LINK;
                    data = 0;
                    f = NULL;
                    h = NULL;
                    break;

                case 'F':
                case 'B': {
                    int color;
                    if(fmt[1] == '\0') goto done;
                    if(fmt[1] == 'p') {
                        color = va_arg(vl, int);
                    } else {
                        color = fmt[1];
                    }
                    if((color < 0 || color > 255) && !(color & 0x80000000)) {
                        color = 0;
                    }
                    if(*fmt == 'F') {
                        fg = color;
                    } else {
                        bg = color;
                    }
                    fmt++;
                    break;
                }
                case 'L':
                    if(fmt[1] == '\0') goto done;
                    fmt++;
                    if(*fmt == 'p') {
                        link = va_arg(vl, int);
                    } else {
                        link = *fmt;
                    }
                    break;

                case 'f':
                    f = va_arg(vl, LinkFunction *);
                    break;

                case 'h':
                    h = va_arg(vl, LinkFunction *);
                    break;

                case 'D':
                    data = va_arg(vl, DWORD);
                    break;
                    
                case '%':
                    strcpy(buf, "%");
                    break;
            }
        } else {
            buf[0] = *fmt;
            buf[1]= '\0';
        }

        for(unsigned i = 0; i < strlen(buf); i++) {
            if(c >= MAX_COLS) goto done;
            text[r][c] = buf[i];
            meta[r][c].fg = fg;
            meta[r][c].bg = bg;
            meta[r][c].link = link;
            meta[r][c].data = data;
            meta[r][c].f = f;
            meta[r][c].h = h;
            c++;
        }

        fmt++;
    }
    while(c < MAX_COLS) {
        meta[r][c].fg = fg;
        meta[r][c].bg = bg;
        c++;
    }

done:
    va_end(vl);
}

#define gs (SS.GW.gs)
void TextWindow::Show(void) {
    if(!(SS.GW.pending.operation)) SS.GW.ClearPending();

    SS.GW.GroupSelection();

    // Make sure these tests agree with test used to draw indicator line on
    // main list of groups screen.
    if(SS.GW.pending.description) {
        // A pending operation (that must be completed with the mouse in
        // the graphics window) will preempt our usual display.
        HideEditControl();
        ShowHeader(false);
        Printf(false, "");
        Printf(false, "%s", SS.GW.pending.description);
        Printf(true, "%Fl%f%Ll(cancel operation)%E",
            &TextWindow::ScreenUnselectAll);
    } else if((gs.n > 0 || gs.constraints > 0) && 
                                    shown.screen != SCREEN_PASTE_TRANSFORMED)
    {
        if(edit.meaning != EDIT_TTF_TEXT) HideEditControl();
        ShowHeader(false);
        DescribeSelection();
    } else {
        if(edit.meaning == EDIT_TTF_TEXT) HideEditControl();
        ShowHeader(true);
        switch(shown.screen) {
            default:
                shown.screen = SCREEN_LIST_OF_GROUPS;
                // fall through
            case SCREEN_LIST_OF_GROUPS:     ShowListOfGroups();     break;
            case SCREEN_GROUP_INFO:         ShowGroupInfo();        break;
            case SCREEN_GROUP_SOLVE_INFO:   ShowGroupSolveInfo();   break;
            case SCREEN_CONFIGURATION:      ShowConfiguration();    break;
            case SCREEN_STEP_DIMENSION:     ShowStepDimension();    break;
            case SCREEN_LIST_OF_STYLES:     ShowListOfStyles();     break;
            case SCREEN_STYLE_INFO:         ShowStyleInfo();        break;
            case SCREEN_PASTE_TRANSFORMED:  ShowPasteTransformed(); break;
            case SCREEN_EDIT_VIEW:          ShowEditView();         break;
            case SCREEN_TANGENT_ARC:        ShowTangentArc();       break;
        }
    }
    Printf(false, "");

    // Make sure there's room for the color picker
    if(editControl.colorPicker.show) {
        int pickerHeight = 25;
        int halfRow = editControl.halfRow;
        if(top[rows-1] - halfRow < pickerHeight && rows < MAX_ROWS) {
            rows++;
            top[rows-1] = halfRow + pickerHeight;
        }
    }

    InvalidateText();
}

void TextWindow::TimerCallback(void)
{
    tooltippedIcon = hoveredIcon;
    InvalidateText();
}

void TextWindow::DrawOrHitTestIcons(int how, double mx, double my)
{
    int width, height;
    GetTextWindowSize(&width, &height);

    int x = 20, y = 33 + LINE_HEIGHT;
    y -= scrollPos*(LINE_HEIGHT/2);

    double grey = 30.0/255;
    double top = y - 28, bot = y + 4;
    glColor4d(grey, grey, grey, 1.0);
    glxAxisAlignedQuad(0, width, top, bot);

    HideShowIcon *oldHovered = hoveredIcon;
    if(how != PAINT) {
        hoveredIcon = NULL;
    }

    HideShowIcon *hsi;
    for(hsi = &(hideShowIcons[0]); hsi->var; hsi++) {
        if(hsi->var == &SPACER) {
            // Draw a darker-grey spacer in between the groups of icons.
            if(how == PAINT) {
                int l = x, r = l + 4,
                    t = y, b = t - 24;
                glColor4d(0.17, 0.17, 0.17, 1);
                glxAxisAlignedQuad(l, r, t, b);
            }
            x += 12;
            continue;
        }

        if(how == PAINT) {
            glPushMatrix();
                glTranslated(x, y-24, 0);
                // Only thing that matters about the color is the alpha,
                // should be one for no transparency
                glColor3d(0, 0, 0);
                glxDrawPixelsWithTexture(hsi->icon, 24, 24);
            glPopMatrix();

            if(hsi == hoveredIcon) {
                glColor4d(1, 1, 0, 0.3);
                glxAxisAlignedQuad(x - 2, x + 26, y + 2, y - 26);
            }
            if(!*(hsi->var)) {
                glColor4d(1, 0, 0, 0.6);
                glLineWidth(2);
                int s = 0, f = 24;
                glBegin(GL_LINES);
                    glVertex2d(x+s, y-s);
                    glVertex2d(x+f, y-f);
                    glVertex2d(x+s, y-f);
                    glVertex2d(x+f, y-s);
                glEnd();
            }
        } else {
            if(mx > x - 2 && mx < x + 26 &&
               my < y + 2 && my > y - 26)
            {
                // The mouse is hovered over this icon, so do the tooltip
                // stuff.
                if(hsi != tooltippedIcon) {
                    oldMousePos = Point2d::From(mx, my);
                }
                if(hsi != oldHovered || how == CLICK) {
                    SetTimerFor(1000);
                }
                hoveredIcon = hsi;
                if(how == CLICK) {
                    SS.GW.ToggleBool(hsi->var);
                }
            }
        }

        x += 32;
    }

    if(how != PAINT && hoveredIcon != oldHovered) {
        InvalidateText();
    }

    if(tooltippedIcon) {
        if(how == PAINT) {
            char str[1024];

            if(tooltippedIcon->icon == Icon_faces) {
                if(SS.GW.showFaces) {
                    strcpy(str, "Don't make faces selectable with mouse");
                } else {
                    strcpy(str, "Make faces selectable with mouse");
                }
            } else {
                sprintf(str, "%s %s", *(tooltippedIcon->var) ? "Hide" : "Show",
                    tooltippedIcon->tip);
            }

            double ox = oldMousePos.x, oy = oldMousePos.y - LINE_HEIGHT;
            ox += 3;
            oy -= 3;
            int tw = (strlen(str) + 1)*CHAR_WIDTH;
            ox = min(ox, (width - 25) - tw);
            oy = max(oy, 5);

            glxCreateBitmapFont();
            glLineWidth(1);
            glColor4d(1.0, 1.0, 0.6, 1.0);
            glxAxisAlignedQuad(ox, ox+tw, oy, oy+LINE_HEIGHT);
            glColor4d(0.0, 0.0, 0.0, 1.0);
            glxAxisAlignedLineLoop(ox, ox+tw, oy, oy+LINE_HEIGHT);

            glColor4d(0, 0, 0, 1);
            glxBitmapText(str, Vector::From(ox+5, oy-3+LINE_HEIGHT, 0));
        } else {
            if(!hoveredIcon ||
                (hoveredIcon != tooltippedIcon))
            {
                tooltippedIcon = NULL;
                InvalidateGraphics();
            }
            // And if we're hovered, then we've set a timer that will cause
            // us to show the tool tip later.
        }
    }
}

//----------------------------------------------------------------------------
// Given (x, y, z) = (h, s, v) in [0,6), [0,1], [0,1], return (x, y, z) =
// (r, g, b) all in [0, 1].
//----------------------------------------------------------------------------
Vector TextWindow::HsvToRgb(Vector hsv) {
    if(hsv.x >= 6) hsv.x -= 6;

    Vector rgb;
    double hmod2 = hsv.x;
    while(hmod2 >= 2) hmod2 -= 2;
    double x = (1 - fabs(hmod2 - 1));
    if(hsv.x < 1) {
        rgb = Vector::From(1, x, 0); 
    } else if(hsv.x < 2) {
        rgb = Vector::From(x, 1, 0); 
    } else if(hsv.x < 3) {
        rgb = Vector::From(0, 1, x); 
    } else if(hsv.x < 4) {
        rgb = Vector::From(0, x, 1); 
    } else if(hsv.x < 5) {
        rgb = Vector::From(x, 0, 1); 
    } else {
        rgb = Vector::From(1, 0, x); 
    }
    double c = hsv.y*hsv.z;
    double m = 1 - hsv.z;
    rgb = rgb.ScaledBy(c);
    rgb = rgb.Plus(Vector::From(m, m, m));

    return rgb;
}

BYTE *TextWindow::HsvPattern2d(void) {
    static BYTE Texture[256*256*3];
    static bool Init;

    if(!Init) {
        int i, j, p;
        p = 0;
        for(i = 0; i < 256; i++) {
            for(j = 0; j < 256; j++) {
                Vector hsv = Vector::From(6.0*i/255.0, 1.0*j/255.0, 1);
                Vector rgb = HsvToRgb(hsv);
                rgb = rgb.ScaledBy(255);
                Texture[p++] = (BYTE)rgb.x;
                Texture[p++] = (BYTE)rgb.y;
                Texture[p++] = (BYTE)rgb.z;
            }
        }
        Init = true;
    }
    return Texture;
}

BYTE *TextWindow::HsvPattern1d(double h, double s) {
    static BYTE Texture[256*4];

    int i, p;
    p = 0;
    for(i = 0; i < 256; i++) {
        Vector hsv = Vector::From(6*h, s, 1.0*(255 - i)/255.0);
        Vector rgb = HsvToRgb(hsv);
        rgb = rgb.ScaledBy(255);
        Texture[p++] = (BYTE)rgb.x;
        Texture[p++] = (BYTE)rgb.y;
        Texture[p++] = (BYTE)rgb.z;
        // Needs a padding byte, to make things four-aligned
        p++;
    }
    return Texture;
}

void TextWindow::ColorPickerDone(void) {
    char str[1024];
    DWORD rgb = editControl.colorPicker.rgb;
    sprintf(str, "%.2f, %.2f, %.3f", REDf(rgb), GREENf(rgb), BLUEf(rgb));
    EditControlDone(str);
}

bool TextWindow::DrawOrHitTestColorPicker(int how, bool leftDown,
                                                double x, double y)
{
    bool mousePointerAsHand = false;

    if(how == HOVER && !leftDown) {
        editControl.colorPicker.picker1dActive = false;
        editControl.colorPicker.picker2dActive = false;
    }

    if(!editControl.colorPicker.show) return false;
    if(how == CLICK || (how == HOVER && leftDown)) InvalidateText();

    static const DWORD BaseColor[12] = {
        RGB(255,   0,   0),
        RGB(  0, 255,   0),
        RGB(  0,   0, 255),

        RGB(  0, 255, 255),
        RGB(255,   0, 255),
        RGB(255, 255,   0),

        RGB(255, 127,   0),
        RGB(255,   0, 127),
        RGB(  0, 255, 127),
        RGB(127, 255,   0),
        RGB(127,   0, 255),
        RGB(  0, 127, 255),
    };

    int width, height;
    GetTextWindowSize(&width, &height);

    int px = LEFT_MARGIN + CHAR_WIDTH*editControl.col;
    int py = (editControl.halfRow - SS.TW.scrollPos)*(LINE_HEIGHT/2);

    py += LINE_HEIGHT + 5;

    static const int WIDTH = 16, HEIGHT = 12;
    static const int PITCH = 18, SIZE = 15;

    px = min(px, width - (WIDTH*PITCH + 40));

    int pxm = px + WIDTH*PITCH + 11,
        pym = py + HEIGHT*PITCH + 7;

    int bw = 6;
    if(how == PAINT) {
        glColor4d(0.2, 0.2, 0.2, 1);
        glxAxisAlignedQuad(px, pxm+bw, py, pym+bw);
        glColor4d(0.0, 0.0, 0.0, 1);
        glxAxisAlignedQuad(px+(bw/2), pxm+(bw/2), py+(bw/2), pym+(bw/2));
    } else {
        if(x < px || x > pxm+(bw/2) ||
           y < py || y > pym+(bw/2))
        {
            return false;
        }
    }
    px += (bw/2);
    py += (bw/2);

    int i, j;
    for(i = 0; i < WIDTH/2; i++) {
        for(j = 0; j < HEIGHT; j++) {
            Vector rgb;
            DWORD d;
            if(i == 0 && j < 8) {
                d = SS.modelColor[j];
                rgb = Vector::From(REDf(d), GREENf(d), BLUEf(d));
            } else if(i == 0) {
                double a = (j - 8.0)/3.0;
                rgb = Vector::From(a, a, a);
            } else {
                d = BaseColor[j];
                rgb = Vector::From(REDf(d), GREENf(d), BLUEf(d));
                if(i >= 2 && i <= 4) {
                    double a = (i == 2) ? 0.2 : (i == 3) ? 0.3 : 0.4;
                    rgb = rgb.Plus(Vector::From(a, a, a));
                }
                if(i >= 5 && i <= 7) {
                    double a = (i == 5) ? 0.7 : (i == 6) ? 0.4 : 0.18;
                    rgb = rgb.ScaledBy(a);
                }
            }

            rgb = rgb.ClampWithin(0, 1);
            int sx = px + 5 + PITCH*(i + 8) + 4, sy = py + 5 + PITCH*j;

            if(how == PAINT) {
                glColor4d(CO(rgb), 1);
                glxAxisAlignedQuad(sx, sx+SIZE, sy, sy+SIZE);
            } else if(how == CLICK) {
                if(x >= sx && x <= sx+SIZE && y >= sy && y <= sy+SIZE) {
                    editControl.colorPicker.rgb = RGBf(rgb.x, rgb.y, rgb.z);
                    ColorPickerDone();
                }
            } else if(how == HOVER) {
                if(x >= sx && x <= sx+SIZE && y >= sy && y <= sy+SIZE) {
                    mousePointerAsHand = true;
                }
            }
        }
    }

    int hxm, hym;
    int hx = px + 5, hy = py + 5;
    hxm = hx + PITCH*7 + SIZE;
    hym = hy + PITCH*2 + SIZE;
    if(how == PAINT) {
        glxColorRGB(editControl.colorPicker.rgb);
        glxAxisAlignedQuad(hx, hxm, hy, hym);
    } else if(how == CLICK) {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            ColorPickerDone();
        }
    } else if(how == HOVER) {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            mousePointerAsHand = true;
        }
    }

    hy += PITCH*3;

    hxm = hx + PITCH*7 + SIZE;
    hym = hy + PITCH*1 + SIZE;
    // The one-dimensional thing to pick the color's value
    if(how == PAINT) {
        glBindTexture(GL_TEXTURE_2D, TEXTURE_COLOR_PICKER_1D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 256, 0,
                     GL_RGB, GL_UNSIGNED_BYTE,
                         HsvPattern1d(editControl.colorPicker.h,
                                      editControl.colorPicker.s));

        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            glTexCoord2d(0, 0);
            glVertex2d(hx, hy);

            glTexCoord2d(1, 0);
            glVertex2d(hx, hym);

            glTexCoord2d(1, 1);
            glVertex2d(hxm, hym);

            glTexCoord2d(0, 1);
            glVertex2d(hxm, hy);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        double cx = hx+(hxm-hx)*(1 - editControl.colorPicker.v);
        glColor4d(0, 0, 0, 1);
        glLineWidth(1);
        glBegin(GL_LINES);
            glVertex2d(cx, hy);
            glVertex2d(cx, hym);
        glEnd();
        glEnd();
    } else if(how == CLICK || 
          (how == HOVER && leftDown && editControl.colorPicker.picker1dActive))
    {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            editControl.colorPicker.v = 1 - (x - hx)/(hxm - hx);

            Vector rgb = HsvToRgb(Vector::From(
                            6*editControl.colorPicker.h,
                            editControl.colorPicker.s,
                            editControl.colorPicker.v));
            editControl.colorPicker.rgb = RGBf(rgb.x, rgb.y, rgb.z);

            editControl.colorPicker.picker1dActive = true;
        }
    }
    // and advance our vertical position
    hy += PITCH*2;

    hxm = hx + PITCH*7 + SIZE;
    hym = hy + PITCH*6 + SIZE;
    // Two-dimensional thing to pick a color by hue and saturation
    if(how == PAINT) {
        glBindTexture(GL_TEXTURE_2D, TEXTURE_COLOR_PICKER_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, HsvPattern2d());

        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            glTexCoord2d(0, 0);
            glVertex2d(hx, hy);

            glTexCoord2d(1, 0);
            glVertex2d(hx, hym);

            glTexCoord2d(1, 1);
            glVertex2d(hxm, hym);

            glTexCoord2d(0, 1);
            glVertex2d(hxm, hy);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        glColor4d(1, 1, 1, 1);
        glLineWidth(1);
        double cx = hx+(hxm-hx)*editControl.colorPicker.h,
               cy = hy+(hym-hy)*editControl.colorPicker.s;
        glBegin(GL_LINES);
            glVertex2d(cx - 5, cy);
            glVertex2d(cx + 4, cy);
            glVertex2d(cx, cy - 5);
            glVertex2d(cx, cy + 4);
        glEnd();
    } else if(how == CLICK || 
          (how == HOVER && leftDown && editControl.colorPicker.picker2dActive))
    {
        if(x >= hx && x <= hxm && y >= hy && y <= hym) {
            double h = (x - hx)/(hxm - hx),
                   s = (y - hy)/(hym - hy);
            editControl.colorPicker.h = h;
            editControl.colorPicker.s = s;

            Vector rgb = HsvToRgb(Vector::From(
                            6*editControl.colorPicker.h,
                            editControl.colorPicker.s,
                            editControl.colorPicker.v));
            editControl.colorPicker.rgb = RGBf(rgb.x, rgb.y, rgb.z);

            editControl.colorPicker.picker2dActive = true;
        }
    }
    
    SetMousePointerToHand(mousePointerAsHand);
    return true;
}

void TextWindow::Paint(void) {
    int width, height;
    GetTextWindowSize(&width, &height);

    // We would like things pixel-exact, to avoid shimmering.
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3d(1, 1, 1);
   
    glTranslated(-1, 1, 0);
    glScaled(2.0/width, -2.0/height, 1);
    // Make things round consistently, avoiding exact integer boundary
    glTranslated(-0.1, -0.1, 0);

    halfRows = height / (LINE_HEIGHT/2);

    int bottom = top[rows-1] + 2;
    scrollPos = min(scrollPos, bottom - halfRows);
    scrollPos = max(scrollPos, 0);

    // Let's set up the scroll bar first
    MoveTextScrollbarTo(scrollPos, top[rows - 1] + 1, halfRows);

    // Create the bitmap font that we're going to use.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    // Now paint the window.
    int r, c, a;
    for(a = 0; a < 2; a++) {
        if(a == 0) {
            glBegin(GL_QUADS);
        } else if(a == 1) {
            glEnable(GL_TEXTURE_2D);
            glxCreateBitmapFont();
            glBegin(GL_QUADS);
        }

        for(r = 0; r < rows; r++) {
            int ltop = top[r];
            if(ltop < (scrollPos-1)) continue;
            if(ltop > scrollPos+halfRows) break;

            for(c = 0; c < min((width/CHAR_WIDTH)+1, MAX_COLS); c++) {
                int x = LEFT_MARGIN + c*CHAR_WIDTH;
                int y = (ltop-scrollPos)*(LINE_HEIGHT/2) + 4;

                int fg = meta[r][c].fg;
                int bg = meta[r][c].bg;

                // On the first pass, all the background quads; on the next
                // pass, all the foreground (i.e., font) quads.
                if(a == 0) {
                    int bh = LINE_HEIGHT, adj = -2;
                    if(bg & 0x80000000) {
                        glColor3f(REDf(bg), GREENf(bg), BLUEf(bg));
                        bh = CHAR_HEIGHT;
                        adj += 2;
                    } else {
                        glColor3fv(&(bgColorTable[bg*3]));
                    }

                    if(!(bg == 'd')) {
                        // Move the quad down a bit, so that the descenders
                        // still have the correct background.
                        y += adj;
                        glxAxisAlignedQuad(x, x + CHAR_WIDTH, y, y + bh);
                        y -= adj;
                    }
                } else if(a == 1) {
                    glColor3fv(&(fgColorTable[fg*3]));
                    glxBitmapCharQuad(text[r][c], x, y + CHAR_HEIGHT);

                    // If this is a link and it's hovered, then draw the
                    // underline
                    if(meta[r][c].link && meta[r][c].link != 'n' &&
                        (r == hoveredRow && c == hoveredCol))
                    {
                        int cs = c, cf = c;
                        while(cs >= 0 && meta[r][cs].link &&
                                         meta[r][cs].f    == meta[r][c].f &&
                                         meta[r][cs].data == meta[r][c].data)
                        {
                            cs--;
                        }
                        cs++;

                        while(          meta[r][cf].link &&
                                        meta[r][cf].f    == meta[r][c].f &&
                                        meta[r][cf].data == meta[r][c].data)
                        {
                            cf++;
                        }

                        // But don't underline checkboxes or radio buttons
                        while((text[r][cs] & 0x80 || text[r][cs] == ' ') &&
                                cs < cf)
                        {
                            cs++;
                        }

                        glEnd();

                        // Always use the color of the rightmost character
                        // in the link, so that underline is consistent color
                        fg = meta[r][cf-1].fg;
                        glColor3fv(&(fgColorTable[fg*3]));
                        glDisable(GL_TEXTURE_2D);
                        glLineWidth(1);
                        glBegin(GL_LINES);
                            int yp = y + CHAR_HEIGHT;
                            glVertex2d(LEFT_MARGIN + cs*CHAR_WIDTH, yp);
                            glVertex2d(LEFT_MARGIN + cf*CHAR_WIDTH, yp);
                        glEnd();

                        glEnable(GL_TEXTURE_2D);
                        glBegin(GL_QUADS);
                    }
                }
            }
        }

        glEnd();
        glDisable(GL_TEXTURE_2D);
    }

    // The line to indicate the column of radio buttons that indicates the
    // active group.
    SS.GW.GroupSelection();
    // Make sure this test agrees with test to determine which screen is drawn
    if(!SS.GW.pending.description && gs.n == 0 && gs.constraints == 0 &&
        shown.screen == SCREEN_LIST_OF_GROUPS)
    {
        int x = 29, y = 70 + LINE_HEIGHT;
        y -= scrollPos*(LINE_HEIGHT/2);

        glLineWidth(1);
        glColor3fv(&(fgColorTable['t'*3]));
        glBegin(GL_LINES);
            glVertex2d(x, y);
            glVertex2d(x, y+40);
        glEnd();
    }

    // The header has some icons that are drawn separately from the text
    DrawOrHitTestIcons(PAINT, 0, 0);

    // And we may show a color picker for certain editable fields
    DrawOrHitTestColorPicker(PAINT, false, 0, 0);
}

void TextWindow::MouseEvent(bool leftClick, bool leftDown, double x, double y) {
    if(TextEditControlIsVisible() || GraphicsEditControlIsVisible()) {
        if(DrawOrHitTestColorPicker(leftClick ? CLICK : HOVER, leftDown, x, y))
        {
            return;
        }

        if(leftClick) {
            HideEditControl();
            HideGraphicsEditControl();
        } else {
            SetMousePointerToHand(false);
        }
        return;
    }

    DrawOrHitTestIcons(leftClick ? CLICK : HOVER, x, y);

    GraphicsWindow::Selection ps = SS.GW.hover;
    SS.GW.hover.Clear();

    int prevHoveredRow = hoveredRow,
        prevHoveredCol = hoveredCol;
    hoveredRow = 0;
    hoveredCol = 0;

    // Find the corresponding character in the text buffer
    int c = (int)((x - LEFT_MARGIN) / CHAR_WIDTH);
    int hh = (LINE_HEIGHT)/2;
    y += scrollPos*hh;
    int r;
    for(r = 0; r < rows; r++) {
        if(y >= top[r]*hh && y <= (top[r]+2)*hh) {
            break;
        }
    }
    if(r >= rows) {
        SetMousePointerToHand(false);
        goto done;
    }

    hoveredRow = r;
    hoveredCol = c;

#define META (meta[r][c])
    if(leftClick) {
        if(META.link && META.f) {
            (META.f)(META.link, META.data);
            Show();
            InvalidateGraphics();
        }
    } else {
        if(META.link) {
            SetMousePointerToHand(true);
            if(META.h) {
                (META.h)(META.link, META.data);
            }
        } else {
            SetMousePointerToHand(false);
        }
    }

done:
    if((!ps.Equals(&(SS.GW.hover))) ||
        prevHoveredRow != hoveredRow ||
        prevHoveredCol != hoveredCol)
    {
        InvalidateGraphics();
        InvalidateText();
    }
}

void TextWindow::MouseLeave(void) {
    tooltippedIcon = NULL;
    hoveredIcon = NULL;
    hoveredRow = 0;
    hoveredCol = 0;
    InvalidateText();
}

void TextWindow::ScrollbarEvent(int newPos) {
    int bottom = top[rows-1] + 2;
    newPos = min(newPos, bottom - halfRows);
    newPos = max(newPos, 0);

    if(newPos != scrollPos) {
        scrollPos = newPos;
        MoveTextScrollbarTo(scrollPos, top[rows - 1] + 1, halfRows);

        if(TextEditControlIsVisible()) {
            ShowEditControl(editControl.halfRow, editControl.col, NULL);
        }
        InvalidateText();
    }
}

