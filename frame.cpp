/*
Copyright 08/05/2024 https://github.com/su8/lunnisWM
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
*/
#include "frame.h"

Frame::Frame(Display *displayPass, Window rootPass, Window clientPass)
{
    client = clientPass;
    display = displayPass;
    root = rootPass;

    //Get attributes of client
    Pixmap color = 0x00feff;
    Pixmap BLACK = 0x000000;
    XWindowAttributes clientAttributes;
    
    XGetWindowAttributes(display, client, &clientAttributes);
    //Make frame
    frameX = clientAttributes.x;
    frameY = clientAttributes.y;
    frameWidth = clientAttributes.width;
    frameHeight = clientAttributes.height + 25;
    frame = XCreateSimpleWindow(display, root, frameX, frameY, frameWidth, frameHeight, 3, color, color);
    
    //Select events for frame
    XSelectInput(display, frame, SubstructureRedirectMask | SubstructureNotifyMask);

    //Grab Keys
    //Grab alt + f4 to frame
    XGrabKey(display, XKeysymToKeycode(display, XK_F4), Mod1Mask, frame, true, GrabModeAsync, GrabModeAsync);
    //Grab alt + button1(left mouse button) to frame
    XReparentWindow(display, client, frame, 0, 25);
    XMoveWindow(display, client, 0, 25);
    //Map Frame
    XMapWindow(display, frame);              

    //Grab alt + button1 for moving windows
    XGrabButton(display, Button1, Mod1Mask, frame, False, ButtonPressMask | ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);

    //Grab button sync mode to grab button press and replaypointer(send) to client window
    XGrabButton(display, Button1, NoEventMask, frame, False, ButtonPressMask | Button1MotionMask, GrabModeSync, GrabModeAsync, None, None);

    //Grab alt + button3 for resizing windows
    XGrabButton(display, Button3, Mod1Mask, frame, False, ButtonPressMask | ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);

    titlebar = XCreateSimpleWindow(display, frame, 0, 0, frameWidth, 25, 0, 0, 0xffffff);
    XMapWindow(display, titlebar);
    XGrabButton(display, Button1, NoEventMask, titlebar, False, Button1MotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);

    closeButton = XCreateSimpleWindow(display, frame, clientAttributes.width - 20, 3, 15, 15, 1, BLACK, color);
    XMapWindow(display, closeButton);

    maximizeButton = XCreateSimpleWindow(display, frame, clientAttributes.width - 45, 3, 15, 15, 1, BLACK, color);
    XMapWindow(display, maximizeButton);

    minimizeButton = XCreateSimpleWindow(display, frame, clientAttributes.width - 70, 3, 15, 15, 1, BLACK, color);
    XMapWindow(display, minimizeButton);
}

Window Frame::getFrameHandle()
{
    return frame;
}

Window Frame::getClientHandle()
{
    return client;
}

Frame::Frame()
{
}

void Frame::moveFrame(int x, int y)
{
    XMoveWindow(display, frame, x, y);
    frameX = x;
    frameY = y;
}

void Frame::moveResizeFrame(int x, int y, int width, int height)
{
    //Move and resize frame
    XMoveResizeWindow(display, frame,
                      x,   // Frame x
                      y,   // Frame y
                      width,  // Frame width
                      height); // Frame height
    //Resize client
    XResizeWindow(display, client, width, height - 25);

    //Move titlebar
    XResizeWindow(display, titlebar, width, 25);

    //Move close button
    XMoveWindow(display, closeButton, width-20, 3);

    //Move maximize button
    XMoveWindow(display, maximizeButton, width-45, 3);

    //Move minimize button
    XMoveWindow(display, minimizeButton, width-70, 3);


//width - 70, 3

    //Change size and location variables
    frameX = x;
    frameY = y;
    frameWidth = width;
    frameHeight = height;
}

void Frame::maximize(int x, int y, int width, int height)
{

    maximized = true;

    //Remove Border
    XSetWindowBorderWidth(display, frame, 0);

    //Move and resize frame
    XMoveResizeWindow(display, frame,
                      x,   // Frame x
                      y,   // Frame y
                      width,  // Frame width
                      height); // Frame height
    //Resize client
    XResizeWindow(display, client, width, height);

    //Move titlebar
    XResizeWindow(display, titlebar, width, 25);

    //Move close button
    XMoveWindow(display, closeButton, width-20, 3);

    //Move maximize button
    XMoveWindow(display, maximizeButton, width-45, 3);

    //Move minimize button
    XMoveWindow(display, minimizeButton, width-70, 3);
}

void Frame::unmaximize()    
{
    maximized = false;

    //Reapply border
    XSetWindowBorderWidth(display, frame, 3);

    //Move and resize frame
    XMoveResizeWindow(display, frame,
                      frameX,   // Frame x
                      frameY,   // Frame y
                      frameWidth,  // Frame width
                      frameHeight); // Frame height
    //Resize client
    XResizeWindow(display, client, frameWidth, frameHeight);

    //Move titlebar
    XResizeWindow(display, titlebar, frameWidth, 25);

    //Move close button
    XMoveWindow(display, closeButton, frameWidth-20, 3);

    //Move maximize button
    XMoveWindow(display, maximizeButton, frameWidth-45, 3);

    //Move minimize button
    XMoveWindow(display, minimizeButton, frameWidth-70, 3);
}

void Frame::handleCloseButton()
{
    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.window = client;
    event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", True);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
    event.xclient.data.l[1] = CurrentTime;
    int result = XSendEvent(display, client, False, NoEventMask, &event);
}

void Frame::handleMaximizeButton(XButtonEvent *ev)
{
    if (!maximized) //if not maximized
    {
        int monitorCnt;
        XRRMonitorInfo *xMonitors = XRRGetMonitors(display, XDefaultRootWindow(display), true, &monitorCnt);

        for (int monitor = 0; monitor < monitorCnt; monitor++)
        {
            if (ev->x_root >= xMonitors[monitor].x &&
                ev->x_root <= xMonitors[monitor].x + xMonitors[monitor].width &&
                ev->y_root >= xMonitors[monitor].y &&
                ev->y_root <= xMonitors[monitor].y + xMonitors[monitor].height) // if (x + 20% width), y inside monitor
            {
                maximize(
                    xMonitors[monitor].x,       //Monitor root x
                    xMonitors[monitor].y,       //Monitot root y
                    xMonitors[monitor].width,   // Monitor width
                    xMonitors[monitor].height); // Monitor height
                break;
            }
        }
    }
    else //if maximized
    {
        unmaximize();
    }
}

void Frame::handleMinimizeButton()
{
    XUnmapWindow(display, frame);
}

void Frame::handleWindowDrag(XMotionEvent *ev, int cursIntX, int cursIntY, int winIntX, int winIntY)
{
    if (maximized)
    {
        if (abs(ev->x_root - cursIntX)< 50 && abs(ev->y_root - cursIntY) < 50)
        {
            return;
        }
        unmaximize();
    }
    moveFrame(
        winIntX + (ev->x_root - cursIntX),
        winIntY + (ev->y_root - cursIntY)
    );
}

void Frame::handleWindowResize(XMotionEvent *ev, int cursIntX, int cursIntY, int winIntX, int winIntY, int winIntWidth, int winIntHeight, int corner)
{
    if (maximized)
    {
        if (abs(ev->x_root - cursIntX)< 50 && abs(ev->y_root - cursIntY) < 50)
        {
            return;
        }
        maximized = false;
        //Reapply border
        XSetWindowBorderWidth(display, frame, 3);
    }
    switch (corner)
        {
            case 0: // top left of window
            {
                const int x = std::min(winIntX + winIntWidth - 50, winIntX + ev->x_root - cursIntX);
                const int y = std::min(winIntY + winIntHeight - 50, winIntY + ev->y_root - cursIntY);
                const int width = std::max(50, winIntWidth + cursIntX - ev->x_root);
                const int height = std::max(50, winIntHeight + cursIntY - ev->y_root);
                moveResizeFrame(x, y, width, height);
            }break;

            case 1: // top right of window
            {
                const int y = std::min(winIntY + winIntHeight - 50, winIntY + ev->y_root - cursIntY);
                const int width = std::max(50, winIntWidth + ev->x_root - cursIntX);
                const int height = std::max(50, winIntHeight + cursIntY - ev->y_root);
            
                moveResizeFrame(winIntX, y, width, height);
            }break;

            case 2: // bottom left of window
            {
                const int x = std::min(winIntX + winIntWidth - 50, winIntX + ev->x_root - cursIntX);
                const int width = std::max(50, winIntWidth + cursIntX - ev->x_root);
                const int height = std::max(50, winIntHeight + ev->y_root - cursIntY);
                moveResizeFrame(x, winIntY, width, height);
            }break;
            
            case 3: // bottom right of window
            {
                const int width = std::max(50, winIntWidth + ev->x_root - cursIntX);
                const int height = std::max(50, winIntHeight + ev->y_root - cursIntY);
                moveResizeFrame(winIntX, winIntY, width, height);
            }break;
        }
}

void Frame::handleWindowDragOrResize(XMotionEvent *ev, int cursIntX, int cursIntY, int winIntX, int winIntY, int winIntWidth, int winIntHeight)
{
    if (ev->window == titlebar)
    {
        handleWindowDrag(ev, cursIntX, cursIntY, winIntX, winIntY);
    }
}

void Frame::handleFrameButtonPress(XButtonEvent *ev)
{
    if (!ev->subwindow)
    {}
    else if (ev->subwindow == closeButton)
    {
        handleCloseButton();
    }
    else if (ev->subwindow == maximizeButton)
    {
        handleMaximizeButton(ev);
    }
    else if (ev->subwindow == minimizeButton)
    {
        handleMinimizeButton();
    }
}
