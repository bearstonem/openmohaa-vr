/*
===========================================================================
Copyright (C) 2025 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// tr_vr.c -- everything the fixed function renderer needs to know about a
// headset, which is deliberately very little. The VR layer owns the session,
// the poses and the swapchains; this is the surface where it meets the
// renderer.

#include "tr_local.h"

vrViewState_t vrView;


/*
============
RE_SetVRView
============
*/
void RE_SetVRView(
    const float *origin,
    const vec3_t *axis,
    float         tanLeft,
    float         tanRight,
    float         tanUp,
    float         tanDown,
    float         baseYaw,
    int           eye
)
{
    if (!origin || !axis) {
        vrView.active = qfalse;
        return;
    }

    vrView.baseYaw = baseYaw;
    vrView.eye     = eye;

    VectorCopy(origin, vrView.origin);
    VectorCopy(axis[0], vrView.axis[0]);
    VectorCopy(axis[1], vrView.axis[1]);
    VectorCopy(axis[2], vrView.axis[2]);

    vrView.tanLeft  = tanLeft;
    vrView.tanRight = tanRight;
    vrView.tanUp    = tanUp;
    vrView.tanDown  = tanDown;
    vrView.active   = qtrue;
}

/*
============
RE_SetDefaultFramebuffer

Called when the VR layer is about to move the target out from under the
renderer, so that anything still owed to the old one is paid first.

Deliberately does not bind anything. This renderer predates framebuffer objects,
has none of its own, and never touches the binding - so there is nothing here to
redirect, and the VR layer's own bind is what selects the eye.

Binding here would in fact be worse than useless. The renderer's GL goes through
gl4es, and gl4es can only bind framebuffers it created itself: its
glBindFramebuffer looks the name up in its own table, and for a name that came
from the driver's glGenFramebuffers - which is where every one of the VR layer's
framebuffers comes from - the lookup misses, it raises GL_INVALID_VALUE and
returns without binding. Worse, it leaves gl4es believing framebuffer 0 is still
current. That was a black headset with the frame loop running at a contented 90
fps, everything drawn neatly into the window nobody was looking at.

What is needed instead is the flush. gl4es batches geometry and issues it
lazily, so work built for one target would otherwise arrive in whichever is
bound when it finally goes out. Flushing while the old target is still bound is
what keeps one eye's contents out of the next.
============
*/
void RE_SetDefaultFramebuffer(unsigned int framebuffer)
{
    if (qglFlush) {
        qglFlush();
    }

    // Drawing into a framebuffer means something else is presenting it, so the
    // window must not also be swapped - two presentation paths running at
    // unrelated rates is what flicker looks like.
    GLimp_SetPresentsToWindow(framebuffer == 0 ? qtrue : qfalse);
}

/*
====================
R_ApplyVRView

Puts the headset on top of whatever camera the game asked for.

Done in the renderer rather than the game because every camera reaches it
through RE_RenderScene - the player's view, cutscene cameras, ladder climbs,
death animations - so head tracking follows all of them without each having to
know about VR.

The game's camera decides where the body is and which way it faces. Everything
else comes from the headset: pitch and roll are taken from the head alone,
since blending them with the game's would fight the viewer's own neck.
====================
*/
void R_ApplyVRView(refdef_t *fd)
{
    vec3_t baseAngles, bodyAngles, bodyAxis[3];
    int    i;

    if (!vrView.active) {
        return;
    }

    // Refuse a head pose that is not a rotation.
    //
    // These axes are written straight into refdef.viewaxis, and R_RotateForViewer
    // copies them into the modelview without checking anything. A zeroed or
    // degenerate set therefore produces a modelview whose determinant is zero -
    // a matrix that collapses three dimensions into a plane - and everything
    // drawn through it disappears while every individual number upstream of it
    // still looks correct. Measured directly: world surfaces reporting
    // "mvdet 0.000" in the same frame as others reporting 1.000.
    //
    // The game's own camera is a perfectly good fallback for a frame; a
    // collapsed one is not.
    {
        const float len0 = VectorLength(vrView.axis[0]);
        const float len1 = VectorLength(vrView.axis[1]);
        const float len2 = VectorLength(vrView.axis[2]);

        if (len0 < 0.5f || len1 < 0.5f || len2 < 0.5f) {
            static int lastWarn;
            int        now = ri.Milliseconds();

            if (now - lastWarn > 1000) {
                lastWarn = now;
                ri.Printf(
                    PRINT_WARNING,
                    "VR: degenerate head pose (axis lengths %.3f %.3f %.3f); keeping the game's camera\n",
                    len0,
                    len1,
                    len2
                );
            }
            return;
        }
    }

    // The game's camera decides where the body stands and which way it faces;
    // the headset decides everything about where the eyes are relative to that.
    // Only the heading is taken from the game - its pitch and roll would fight
    // the viewer's own neck.
    //
    // Minus whatever heading the client already fed into the view angles. The
    // game is told where the player is looking so that objectives and anything
    // else consulting the view angles agree with what is in front of them, but
    // that same heading then comes back through the player state and into this
    // camera - and composing the head onto it again turns it twice. Yaw ends up
    // doubled while pitch, which the game's camera does not carry here, does
    // not, and half of the doubled yaw arrives a frame late through prediction.
    // The result reads as a world that swims when the head turns.
    //
    // Taking it back off leaves the game's own contributions - recoil, lean,
    // cutscene cameras, ladders - composed with the current headset pose, which
    // is what this function is for.
    vectoangles(fd->viewaxis[0], baseAngles);
    VectorSet(bodyAngles, 0, baseAngles[YAW] - vrView.baseYaw, 0);
    AnglesToAxis(bodyAngles, bodyAxis);

    // Rotate the head's axes into the body's frame. Composed as a rotation
    // rather than by adding Euler angles: adding them only happens to work
    // while pitch and roll are small, and turns every head movement into a
    // twist once they are not.
    for (i = 0; i < 3; i++) {
        vec3_t rotated;

        VectorScale(bodyAxis[0], vrView.axis[i][0], rotated);
        VectorMA(rotated, vrView.axis[i][1], bodyAxis[1], rotated);
        VectorMA(rotated, vrView.axis[i][2], bodyAxis[2], rotated);
        VectorCopy(rotated, fd->viewaxis[i]);
    }

    if (r_vrTrace && r_vrTrace->integer) {
        static int lastTrace;
        int        now = ri.Milliseconds();

        if (now - lastTrace > 1000) {
            lastTrace = now;
            ri.Printf(
                PRINT_ALL,
                "VR view: org %.0f %.0f %.0f body yaw %.1f | head off %.1f %.1f %.1f fwd %.2f %.2f %.2f\n",
                fd->vieworg[0],
                fd->vieworg[1],
                fd->vieworg[2],
                baseAngles[YAW],
                vrView.origin[0],
                vrView.origin[1],
                vrView.origin[2],
                vrView.axis[0][0],
                vrView.axis[0][1],
                vrView.axis[0][2]
            );
        }
    }

    // The head's offset within the play space, turned to face the same way the
    // body does before it is added. Leaning left stays leaning left whichever
    // way the player has turned.
    VectorMA(fd->vieworg, vrView.origin[0], bodyAxis[0], fd->vieworg);
    VectorMA(fd->vieworg, vrView.origin[1], bodyAxis[1], fd->vieworg);
    VectorMA(fd->vieworg, vrView.origin[2], bodyAxis[2], fd->vieworg);
}

/*
================
R_VRGet2DTransform

The one correction flat content needs in a headset, as a scale about the centre
of the eye buffer and an offset in half-widths of it.

Three things are wrong with 2D in a headset that are not wrong on a screen. Each
eye looks through a frustum that is not centred on its own axis, so identical 2D
lands in a different place in each eye; identical 2D also has no disparity, so
whatever does line up sits at infinity rather than in front of the player; and
the edges of the display are outside what can comfortably be read.

Returns false when there is nothing to correct, which is every flat frame and
every pass that draws onto a compositor quad - the wrist panel and the screen
layer both have the compositor place them in the room, so applying an eye's
frustum to them would be doing the same job twice, oppositely.
================
*/
static qboolean R_VRGet2DTransform(float *scale, float *offX, float *offY)
{
    float tanWidth, tanHeight;
    float depth, parallax;

    if (!vrView.active) {
        return qfalse;
    }

    tanWidth  = vrView.tanRight - vrView.tanLeft;
    tanHeight = vrView.tanUp - vrView.tanDown;

    // No frustum yet means nothing to centre on, and dividing by it would leave
    // a NaN in the projection - which does not look like an error, it looks like
    // the HUD simply not being there.
    if (tanWidth < 0.0001f || tanHeight < 0.0001f) {
        return qfalse;
    }

    *scale = vr_hudScale ? vr_hudScale->value : 1.0f;
    depth  = vr_hudDepth ? vr_hudDepth->value : 2.0f;

    if (*scale < 0.1f) {
        *scale = 0.1f;
    } else if (*scale > 1.0f) {
        *scale = 1.0f;
    }
    if (depth < 0.5f) {
        depth = 0.5f;
    }

    // Where the eye's optical axis falls in its own buffer, as a share of half
    // the buffer. Zero tangent is straight ahead; the runtime's frustum is
    // asymmetric, so that is not the middle of the image.
    *offX = -(vrView.tanRight + vrView.tanLeft) / tanWidth;
    *offY = -(vrView.tanUp + vrView.tanDown) / tanHeight;

    //
    // Added in OPM
    //  How much of the frustum centring to apply. 1 puts the content on the
    //  optical axis, which is what makes the two eyes agree; 0 centres it on
    //  the buffer instead, which is wrong but symmetric.
    //
    //  Kept as a knob because it is the term that decides fusion: it is equal
    //  and opposite in the two eyes, so an error here pulls the images apart
    //  rather than merely misplacing them. Anything other than 1 is a way of
    //  asking whether the tangents are being reported honestly, not a setting.
    //
    if (vr_hudOffset) {
        *offX *= vr_hudOffset->value;
        *offY *= vr_hudOffset->value;
    }

    // Half an interpupillary distance at the given range, as a share of the
    // eye's half width, opposite ways round for the two eyes. This is what puts
    // the HUD at that range instead of infinitely far away.
    parallax = (2.0f * 0.032f) / (depth * tanWidth);
    *offX += (vrView.eye == 0) ? parallax : -parallax;

    //
    // Added in OPM
    //  One line per eye, once. Convergence cannot be judged from a screenshot -
    //  the Quest's capture is a reprojected composite of one eye, so the two
    //  images that have to fuse are never both in it. These are the numbers
    //  that decide it, and the last of them is the whole answer: the disparity
    //  is how far apart the same pixel lands in the two eyes, and it is either
    //  the half-IPD the depth asks for or it is not.
    //
    {
        static int reported;

        if (vr_traceHud && vr_traceHud->integer) {
            if (!(reported & (1 << vrView.eye))) {
                reported |= 1 << vrView.eye;
                ri.Printf(
                    PRINT_ALL,
                    "vr hud: eye %d tan L%.3f R%.3f U%.3f D%.3f | scale %.2f depth %.2f offsetCvar %.2f"
                    " | offX %.4f offY %.4f parallax %.4f | shift %.1f,%.1f px of %dx%d\n",
                    vrView.eye,
                    vrView.tanLeft, vrView.tanRight, vrView.tanUp, vrView.tanDown,
                    *scale, depth, vr_hudOffset ? vr_hudOffset->value : 1.0f,
                    *offX, *offY, parallax,
                    *offX * glConfig.vidWidth * 0.5f,
                    *offY * glConfig.vidHeight * 0.5f,
                    glConfig.vidWidth, glConfig.vidHeight
                );
            }
        } else {
            // Re-armed while it is off, so turning it on later still says
            // something. Otherwise the one report is spent during startup.
            reported = 0;
        }
    }

    return qtrue;
}

/*
================
R_VRAdjust2DRect

Moves a 2D viewport or scissor rectangle to suit the eye it is about to be drawn
into. Coordinates are pixels in the eye buffer, GL's way up.

Expressed as a change to the rectangle rather than to the ortho window, and this
is the whole point of the function.

The correction is a property of the display: the same number of pixels, in the
same direction, for everything flat in the frame. An ortho window is not - in
this engine it is usually a single widget. UIWidget::set2D gives every widget a
viewport and an ortho window cut to that widget's own frame, and the HUD is
built out of widgets: hud_compass, hud_health, hud_ammo, hud_weapons, hud_items
are each a menu drawn that way. Correcting the window therefore scaled the
correction to the widget, and a hundred and fifty pixel compass in a sixteen
hundred pixel eye received about a twelfth of the shift it needed.

That is what the HUD refusing to fuse actually was. Every element ended up with
a disparity in proportion to its own width, so each one sat at a different
apparent depth - the objectives, drawn through the full screen window the 3D
view sets up, converged where they were asked to; the compass beside them sat
near infinity. There is no vergence a pair of eyes can adopt that resolves both,
so the HUD reads as doubled no matter what vr_hudDepth is set to.

Applied to the rectangle it comes out in pixels, which every 2D window shares,
so all of it converges together whatever window it was drawn through. It also
puts vr_hudScale back in charge of where things are rather than only how big
they are: scaling about the centre of the display draws the corners inwards,
where before each widget merely shrank where it stood.
================
*/
void R_VRAdjust2DRect(int *x, int *y, int *w, int *h)
{
    float scale, offX, offY;
    float halfW, halfH;
    float x0, y0, x1, y1;

    if (!R_VRGet2DTransform(&scale, &offX, &offY)) {
        return;
    }

    halfW = glConfig.vidWidth * 0.5f;
    halfH = glConfig.vidHeight * 0.5f;

    // Both edges are mapped and then rounded, rather than rounding the origin
    // and scaling the size, so that neighbouring rectangles stay neighbouring.
    x0 = halfW + (*x - halfW) * scale + offX * halfW;
    y0 = halfH + (*y - halfH) * scale + offY * halfH;
    x1 = x0 + *w * scale;
    y1 = y0 + *h * scale;

    *x = (int)floor(x0 + 0.5f);
    *y = (int)floor(y0 + 0.5f);
    *w = (int)floor(x1 + 0.5f) - *x;
    *h = (int)floor(y1 + 0.5f) - *y;

    // A zero width viewport is undefined and a zero width scissor draws
    // nothing; a widget small enough to round away is better drawn thin.
    if (*w < 1) {
        *w = 1;
    }
    if (*h < 1) {
        *h = 1;
    }
}
