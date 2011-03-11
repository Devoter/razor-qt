#ifndef XFITMAN_CPP
#define XFITMAN_CPP

#define dbg

#include <QtGui/QX11Info>
#include <QtCore/QList>
#include <QtGui/QApplication>
#include <QtCore/QDebug>
#include <QtGui/QDesktopWidget>

#include <stdint.h>
#include "xfitman2.h"

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

/**
 * @file xfitman.cpp
 * @brief implements class Xfitman
 * @author Christopher "VdoP" Regali
 */

/*
 Some requests from Clients include type of the Client, for example the _NET_ACTIVE_WINDOW
 message. Currently the types can be 1 for normal applications, and 2 for pagers.
 See http://standards.freedesktop.org/wm-spec/latest/ar01s09.html#sourceindication
 */
#define SOURCE_NORMAL   1
#define SOURCE_PAGER    2

/*
  _NET_WM_STATE actions
 */
#define _NET_WM_STATE_REMOVE    0    // remove/unset property
#define _NET_WM_STATE_ADD       1    // add/set property
#define _NET_WM_STATE_TOGGLE    2    // toggle property


const XfitMan2&  xfitMan2()
{
    static XfitMan2 instance;
    return instance;
}

/**
 * @brief constructor: gets Display vars and registers us
 */
XfitMan2::XfitMan2()
{
    getAtoms();
    root = QX11Info::appRootWindow();
    screencount = ScreenCount(QX11Info::display());
}

/**
 * @brief moves a window to a new position
 */

void XfitMan2::moveWindow(Window _win, int _x, int _y) const
{
    XMoveWindow(QX11Info::display(), _win, _x, _y);
}


/**
 * @brief this sets our background to the pixmap map
 */
void XfitMan2::setRootBackground(QPixmap _map) const
{
    Pixmap p = _map.handle();
    XGrabServer(QX11Info::display());
    XChangeProperty(QX11Info::display(), root, atomMap["xrootpmap"], XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &p, 1);
    XChangeProperty(QX11Info::display(), root, atomMap["esetroot"], XA_PIXMAP, 32, PropModeReplace, (unsigned char *) &p, 1);
    XSetCloseDownMode(QX11Info::display(), RetainPermanent);
    XSetWindowBackgroundPixmap(QX11Info::display(), root, p);
    XClearWindow(QX11Info::display(), root);
    XUngrabServer(QX11Info::display());
    XFlush(QX11Info::display());
}


/************************************************

 ************************************************/
bool XfitMan2::getWindowProperty(Window window,
                       Atom atom,               // property
                       Atom reqType,            // req_type
                       unsigned long* resultLen,// nitems_return
                       unsigned char** result   // prop_return
                      ) const
{
    int  format;
    unsigned long type, rest;
    return XGetWindowProperty(QX11Info::display(), window, atom, 0, 4096, false,
                              reqType, &type, &format, resultLen, &rest,
                              result)  == Success;
}


/************************************************

 ************************************************/
bool XfitMan2::getRootWindowProperty(Atom atom,    // property
                           Atom reqType,            // req_type
                           unsigned long* resultLen,// nitems_return
                           unsigned char** result   // prop_return
                          ) const
{
    return getWindowProperty(root, atom, reqType, resultLen, result);
}


/**
 * @brief this one gets the active application window.
 */
Window XfitMan2::getActiveAppWindow() const
{
    Window window = getActiveWindow();
    if (window == 0)
        return 0;

    if (acceptWindow(window))
        return window;

    Window transFor = None;
    if (XGetTransientForHint(QX11Info::display(), window, &transFor))
        return transFor;

    return 0;
}


/**
 * @brief this makes the wm send Windowevents to us which normally do not belong to zs
 */

void XfitMan2::setEventRoute() const
{
    XSelectInput(QX11Info::display(), root, StructureNotifyMask | SubstructureNotifyMask);
}

/**
 * @brief returns the window that currently has inputfocus
 */
Window XfitMan2::getActiveWindow() const
{
    unsigned long len;
    unsigned long *data;
    if (!getWindowProperty(root, atom("_NET_ACTIVE_WINDOW"), XA_WINDOW,
                          &len, (unsigned char**) &data)
       )
        return 0;

    Window result = 0;
    if (len)
        result = data[0];

    XFree(data);
    return result;
}


/**
 * @brief Get the number of desktops
 */

int XfitMan2::getNumDesktop() const
{
    unsigned long length, *data;
    getRootWindowProperty(atom("_NET_NUMBER_OF_DESKTOPS"), XA_CARDINAL, &length, (unsigned char**) &data);
    int res = data[0];
    XFree(data);
    return res;
}

/**
 * @brief resizes a window to the given dimensions
 */
void XfitMan2::resizeWindow(Window _wid, int _width, int _height) const
{
    XResizeWindow(QX11Info::display(), _wid, _width, _height);
}



/**
 * @brief gets a windowpixmap from a window
 */

bool XfitMan2::getClientIcon(Window _wid, QPixmap& _pixreturn) const
{
    int format;
    ulong type, nitems, extra;
    ulong* data = 0;

    XGetWindowProperty(QX11Info::display(), _wid, atomMap["net_wm_icon"],
                       0, LONG_MAX, False, AnyPropertyType,
                       &type, &format, &nitems, &extra,
                       (uchar**)&data);
    if (!data)
    {
        qDebug() << "Cannot obtain pixmap info from the window";
        return false;
    }

    QImage img (data[0], data[1], QImage::Format_ARGB32);
    for (int i=0; i<img.byteCount()/4; ++i)
        ((uint*)img.bits())[i] = data[i+2];

    _pixreturn = QPixmap::fromImage(img);
    XFree(data);

    return true;
}



/**
 * @brief destructor
 */
XfitMan2::~XfitMan2()
{
}
/**
 * @brief returns a windowname and sets _nameSource to the finally used Atom
 */

//i got the idea for this from taskbar-plugin of LXPanel - so credits fly out :)
QString XfitMan2::getName(Window _wid) const
{
    QString name = "";
    //first try the modern net-wm ones
    unsigned long length;
    unsigned char *data = NULL;
    Atom utf8Atom = atom("UTF8_STRING");

    if (getWindowProperty(_wid, atom("_NET_WM_VISIBLE_NAME"), utf8Atom, &length, &data))
    {
        name = QString::fromUtf8((char*) data);
        XFree(data);

    }

    if (name.isEmpty())
        if (getWindowProperty(_wid, atom("_NET_WM_NAME"), utf8Atom, &length, &data))
        {
            name = QString::fromUtf8((char*) data);
            XFree(data);
        }

    if (name.isEmpty())
        if (getWindowProperty(_wid, atom("XA_WM_NAME"), XA_STRING, &length, &data))
        {
            name = (char*) data;
            XFree(data);
        }

    if (name.isEmpty())
    {
        Status ok = XFetchName(QX11Info::display(), _wid, (char**) &data);
        name = QString((char*) data);
        if (0 != ok) XFree(data);
    }

    return name;
}


/**
 * @brief this add(1) / removes (0) / toggles (2) the _NET_WM_STATE_XXXX flag for a
 *  specified window
 * @param[in] _wid windowId for the action
 * @param[in] _atomcode the QString-code for atomMap - state
 * @param[in] _action the action to do (add 1, remove 0, toggle 2)
 */
void XfitMan2::setClientStateFlag(Window _wid, QString _atomcode, int _action) const
{
    clientMessage(_wid, atomMap["net_wm_state"],_action,atomMap[_atomcode],0,0,0);
}


/**
 * @brief sends a clientmessage to a window
 */
int XfitMan2::clientMessage(Window _wid, Atom _msg,
                            unsigned long data0,
                            unsigned long data1,
                            unsigned long data2,
                            unsigned long data3,
                            unsigned long data4) const
{
    XClientMessageEvent msg;
    msg.window = _wid;
    msg.type = ClientMessage;
    msg.message_type = _msg;
    msg.send_event = true;
    msg.display = QX11Info::display();
    msg.format = 32;
    msg.data.l[0] = data0;
    msg.data.l[1] = data1;
    msg.data.l[2] = data2;
    msg.data.l[3] = data3;
    msg.data.l[4] = data4;
    if (XSendEvent(QX11Info::display(), root, FALSE, (SubstructureRedirectMask | SubstructureNotifyMask) , (XEvent *) &msg) == Success)
        return EXIT_SUCCESS;
    else
        return EXIT_FAILURE;
}


void XfitMan2::mapRaised(Window _wid) const
{
    XMapRaised(QX11Info::display(), _wid);
}


/***********************************************

 ***********************************************/
WindowAllowedActions XfitMan2::getAllowedActions(Window window) const
{
    WindowAllowedActions actions = { };

    unsigned long len;
    unsigned long *data;
    if (getWindowProperty(window, atom("_NET_WM_ALLOWED_ACTIONS"), XA_ATOM, &len, (unsigned char**) &data))
    {
        for (unsigned long i=0; i<len; ++i)
        {
            if (data[i] == atom("_NET_WM_ACTION_MOVE"))             actions.Move = true;            else
            if (data[i] == atom("_NET_WM_ACTION_RESIZE"))           actions.Resize = true;          else
            if (data[i] == atom("_NET_WM_ACTION_MINIMIZE"))         actions.Minimize = true;        else
            if (data[i] == atom("_NET_WM_ACTION_SHADE"))            actions.Shade = true;           else
            if (data[i] == atom("_NET_WM_ACTION_STICK"))            actions.Stick = true;           else
            if (data[i] == atom("_NET_WM_ACTION_MAXIMIZE_HORZ"))    actions.MaximizeHoriz = true;   else
            if (data[i] == atom("_NET_WM_ACTION_MAXIMIZE_VERT"))    actions.MaximizeVert = true;    else
            if (data[i] == atom("_NET_WM_ACTION_FULLSCREEN"))       actions.FullScreen = true;      else
            if (data[i] == atom("_NET_WM_ACTION_CHANGE_DESKTOP"))   actions.ChangeDesktop = true;   else
            if (data[i] == atom("_NET_WM_ACTION_CLOSE"))            actions.Close = true;           else
            if (data[i] == atom("_NET_WM_ACTION_ABOVE"))            actions.AboveLayer = true;      else
            if (data[i] == atom("_NET_WM_ACTION_BELOW"))            actions.BelowLayer = true;
        }
        XFree(data);
    }

    return actions;
}


WindowState XfitMan2::getWindowState(Window window) const
{
    WindowState state = { };

    unsigned long len;
    unsigned long *data;
    if (getWindowProperty(window, atom("_NET_WM_STATE"), XA_ATOM, &len, (unsigned char**) &data))
    {
        for (unsigned long i=0; i<len; ++i)
        {
            if (data[i] == atom("_NET_WM_STATE_MODAL"))             state.Modal = true;             else
            if (data[i] == atom("_NET_WM_STATE_STICKY"))            state.Sticky = true;            else
            if (data[i] == atom("_NET_WM_STATE_MAXIMIZED_VERT"))    state.MaximizedVert = true;     else
            if (data[i] == atom("_NET_WM_STATE_MAXIMIZED_HORZ"))    state.MaximizedHoriz = true;    else
            if (data[i] == atom("_NET_WM_STATE_SHADED"))            state.Shaded = true;            else
            if (data[i] == atom("_NET_WM_STATE_SKIP_TASKBAR"))      state.SkipTaskBar = true;       else
            if (data[i] == atom("_NET_WM_STATE_SKIP_PAGER"))        state.SkipPager = true;         else
            if (data[i] == atom("_NET_WM_STATE_HIDDEN"))            state.Hidden = true;            else
            if (data[i] == atom("_NET_WM_STATE_FULLSCREEN"))        state.FullScreen = true;        else
            if (data[i] == atom("_NET_WM_STATE_ABOVE"))             state.AboveLayer = true;        else
            if (data[i] == atom("_NET_WM_STATE_BELOW"))             state.BelowLayer = true;        else
            if (data[i] == atom("_NET_WM_STATE_DEMANDS_ATTENTION")) state.Attention = true;
        }
        XFree(data);
    }

    return state;

}



/**
 * @brief returns true if a window has its hidden_flag set
 */

bool XfitMan2::isHidden(Window _wid) const
{
    return getWindowState(_wid).Hidden;
}


bool XfitMan2::requiresAttention(Window _wid) const
{
    return getWindowState(_wid).Attention;
}


Atom XfitMan2::atom(const char* atomName)
{
    static QHash<QString, Atom> hash;

    if (hash.contains(atomName))
        return hash.value(atomName);

    Atom atom = XInternAtom(QX11Info::display(), atomName, false);
    hash[atomName] = atom;
    return atom;
}


/**
 * @brief gets the used atoms into a QMap for further usage
 */
void XfitMan2::getAtoms() const
{
    atomMap["net_wm_strut"] = XInternAtom(QX11Info::display(), "_NET_WM_STRUT", False);
    atomMap["net_wm_strut_partial"] = XInternAtom(QX11Info::display(), "_NET_WM_STRUT_PARTIAL", False);
    atomMap["net_client_list"] = XInternAtom(QX11Info::display(), "_NET_CLIENT_LIST", False);
    atomMap["net_wm_visible_name"] =XInternAtom(QX11Info::display(), "_NET_WM_VISIBLE_NAME", False);
    atomMap["net_wm_name"] =XInternAtom(QX11Info::display(), "_NET_WM_NAME", False);
    atomMap["xa_wm_name"] =XInternAtom(QX11Info::display(), "XA_WM_NAME", False);
    atomMap["utf8"] = XInternAtom(QX11Info::display(), "UTF8_STRING", False);
    atomMap["net_wm_icon"] = XInternAtom(QX11Info::display(), "_NET_WM_ICON", False);
    atomMap["net_wm_state_hidden"] = XInternAtom(QX11Info::display(), "_NET_WM_STATE_HIDDEN", False);
    atomMap["net_wm_state_fullscreen"] = XInternAtom(QX11Info::display(), "_NET_WM_STATE_FULLSCREEN", False);
    atomMap["net_wm_state"] = XInternAtom(QX11Info::display(), "_NET_WM_STATE", False);
    atomMap["net_current_desktop"] = XInternAtom(QX11Info::display(), "_NET_CURRENT_DESKTOP", False);
    atomMap["net_wm_desktop"] = XInternAtom(QX11Info::display(),"_NET_WM_DESKTOP", False);
    atomMap["net_active_window"] = XInternAtom(QX11Info::display(), "_NET_ACTIVE_WINDOW", False);
    atomMap["_win_workspace"] = XInternAtom(QX11Info::display(), "_WIN_WORKSPACE", False);
    atomMap["net_number_of_desktops"] = XInternAtom(QX11Info::display(), "_NET_NUMBER_OF_DESKTOPS", False);

    atomMap["net_wm_window_type"] = XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE", False);
    atomMap["net_wm_window_type_normal"] = XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE_NORMAL", False);
    atomMap["net_wm_window_type_desktop"] = XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    atomMap["net_wm_window_type_dock"] = XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE_DOCK", False);
    atomMap["net_wm_window_type_splash"] = XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE_SPLASH", False);
    atomMap["kde_net_wm_window_type_override"] = XInternAtom(QX11Info::display(), "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE", False);

    char atomname[20] = {0};
    qsnprintf(atomname, 20, "_NET_SYSTEM_TRAY_S%d", DefaultScreen(QX11Info::display()));
    atomMap["net_system_tray"] = XInternAtom(QX11Info::display(), atomname, False);
    atomMap["net_system_tray_opcode"] = XInternAtom(QX11Info::display(), "_NET_SYSTEM_TRAY_OPCODE", False);
    atomMap["net_manager"] = XInternAtom(QX11Info::display(), "MANAGER", False);
    atomMap["net_message_data"] = XInternAtom(QX11Info::display(), "_NET_SYSTEM_TRAY_MESSAGE_DATA", False);
    atomMap["xrootpmap"] = XInternAtom(QX11Info::display(), "_XROOTPMAP_ID", False);
    atomMap["esetroot"] = XInternAtom(QX11Info::display(), "ESETROOT_PMAP_ID", False);
    atomMap["net_wm_window_demands_attention"] = XInternAtom(QX11Info::display(), "_NET_WM_STATE_DEMANDS_ATTENTION", False);

}

AtomList XfitMan2::getWindowType(Window window) const
{
    AtomList result;

    unsigned long length, *data;
    length=0;
    if (!getWindowProperty(window, atom("_NET_WM_WINDOW_TYPE"), (Atom)AnyPropertyType, &length, (unsigned char**) &data))
        return result;

    for (unsigned int i = 0; i < length; i++)
        result.append(data[i]);

    XFree(data);
    return result;
}


/**
 * @brief rejects a window from beeing listed
 */
bool XfitMan2::acceptWindow(Window window) const
{
    {
        AtomList types = getWindowType(window);

        if (types.contains(atom("_NET_WM_WINDOW_TYPE_DESKTOP")))      return false;
        if (types.contains(atom("_NET_WM_WINDOW_TYPE_DOCK")))         return false;
        if (types.contains(atom("_NET_WM_WINDOW_TYPE_SPLASH")))       return false;
        if (types.contains(atom("_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"))) return false;
    }

    Window transFor = None;
    // WM_TRANSIENT_FOR hint not set - normal window
    if (!XGetTransientForHint(QX11Info::display(), window, &transFor))
        return true;

    if (transFor == 0)      return true;
    if (transFor == window) return true;
    if (transFor == root)   return true;

     AtomList transForTypes = getWindowType(transFor);
     return !transForTypes.contains(atom("_NET_WM_WINDOW_TYPE_NORMAL"));
}



/**
 * @brief gets a client list
 */
QList<Window> XfitMan2::getClientList() const
{
    //initialize the parameters for the XGetWindowProperty call
    unsigned long length, *data;
    length=0;

    /**
     * @todo maybe support multiple desktops here!
     */
    QList<Window> output;

    if (getRootWindowProperty(atom("_NET_CLIENT_LIST"), (Atom)AnyPropertyType, &length,  (unsigned char**) &data))
    {
        for (unsigned int i = 0; i < length; i ++)
            output.append(data[i]);
        XFree(data);
    }

    return output;
}


/**
 * @brief returns the current desktop
 */
int XfitMan2::getActiveDesktop() const
{
    int res = -2;
    unsigned long length, *data;
    if (getRootWindowProperty(atom("_NET_CURRENT_DESKTOP"), XA_CARDINAL, &length, (unsigned char**) &data))
    {
        res = data[0];
        XFree(data);
    }

    return res;
}


/**
 * @brief gets the desktop of the windows _wid
 */
int XfitMan2::getWindowDesktop(Window _wid) const
{
    int  res = -1;
    unsigned long length, *data;
    // so we try to use net_wm_desktop first, but if the
    // system does not use net_wm standard we use win_workspace!
    if (getWindowProperty(_wid, atom("_NET_WM_DESKTOP"), XA_CARDINAL, &length, (unsigned char**) &data))
    {
        res = data[0];
        XFree(data);
    }
    else
    {
        if (getWindowProperty(_wid, atom("_WIN_WORKSPACE"), XA_CARDINAL, &length, (unsigned char**) &data))
        {
            res = data[0];
            XFree(data);
        }
    }

    return res;
}


/**
 * @brief moves a window to a specified desktop
 */

void XfitMan2::moveWindowToDesktop(Window _wid, int _display) const
{
    clientMessage(_wid,atomMap["net_wm_desktop"],(unsigned long) _display,0,0,0,0);
}


/**
 * @brief raises windows _wid
 */
void XfitMan2::raiseWindow(Window _wid) const
{
    clientMessage(_wid, atom("_NET_ACTIVE_WINDOW"),
                  SOURCE_PAGER);
}


/************************************************

 ************************************************/
void XfitMan2::minimizeWindow(Window _wid) const
{
    clientMessage(_wid, atom("WM_CHANGE_STATE"),
                  IconicState);
}


/************************************************

 ************************************************/
void XfitMan2::maximizeWindow(Window _wid) const
{
    clientMessage(_wid, atom("_NET_WM_STATE"),
                  _NET_WM_STATE_ADD,
                  atom("_NET_WM_STATE_MAXIMIZED_VERT"),
                  atom("_NET_WM_STATE_MAXIMIZED_HORZ"),
                  SOURCE_PAGER);
    raiseWindow(_wid);
}


/************************************************

 ************************************************/
void XfitMan2::shadeWindow(Window _wid, bool shade) const
{
    clientMessage(_wid, atom("_NET_WM_STATE"),
                  shade ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
                  atom("_NET_WM_STATE_SHADED"),
                  0,
                  SOURCE_PAGER);
}


/************************************************

 ************************************************/
void XfitMan2::closeWindow(Window _wid) const
{
    clientMessage(_wid, atom("_NET_CLOSE_WINDOW"),
                  0, // Timestamp
                  SOURCE_PAGER);
}


/**
 * @brief changes active desktop to _desktop
 */
void XfitMan2::setActiveDesktop(int _desktop) const
{
    clientMessage(root,atomMap["net_current_desktop"],(unsigned long) _desktop,0,0,0,0);
}


/**
 * @brief this sets a window as selection owner for a specified atom - checks for success then sends the clientmessage
 */
void XfitMan2::setSelectionOwner(Window _wid, QString _selection,QString _manager) const
{
    XSetSelectionOwner(QX11Info::display(), atomMap.value(_selection), _wid, CurrentTime);
    if (getSelectionOwner(_selection)== _wid)
        clientMessage(QApplication::desktop()->winId(),atomMap.value(_manager),CurrentTime,atomMap.value(_selection),_wid,0,0);
}

/**
 * @brief returns the owning window of selection _selection
 */
Window XfitMan2::getSelectionOwner(QString _selection) const
{
    return XGetSelectionOwner(QX11Info::display(), atomMap.value(_selection));
}


/**
 * @brief sets net_wm_strut_partial = our reserved panelspace for the mainbar!
 */
void XfitMan2::setStrut(Window _wid,
                       int left, int right,
                       int top,  int bottom,

                       int leftStartY,   int leftEndY,
                       int rightStartY,  int rightEndY,
                       int topStartX,    int topEndX,
                       int bottomStartX, int bottomEndX
                       ) const
{
    //qDebug() << "XfitMan2: Trying to set STRUT_PARTIAL for panel!";
    //prepare strutsize
    memset(desstrut,0,sizeof(desstrut));
    //prepare the array
    //it has format:
    /*
     * left, right, top, bottom, left_start_y, left_end_y,
     * right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x,
     * bottom_end_x
     *
     */

    //so we take our panelsize from the bottom up
    desstrut[0] = left; desstrut[1] = right;
    desstrut[2] = top;  desstrut[3] = bottom;

    desstrut[4] = leftStartY;    desstrut[5] = leftEndY;
    desstrut[6] = rightStartY;   desstrut[7] = rightEndY;
    desstrut[8] = topStartX;     desstrut[9] = topEndX;
    desstrut[10] = bottomStartX; desstrut[11] = bottomEndX;

    //now we can change that property right
    XChangeProperty(QX11Info::display(), _wid , atom("_NET_WM_STRUT_PARTIAL"),
                    XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) desstrut, 12  );

    //now some wm do not support net_wm_strut_partial but only net_wm_strut, so we also
    // send that one too xdg-std says: if you get a strut_partial ignore all following
    // struts! so if this msg is recognized its ok if not, we dont care either

    XChangeProperty(QX11Info::display(), _wid, atom("_NET_WM_STRUT"),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) desstrut, 4);
}


/**
 * @brief this unsets the strut set for panel
 */
void XfitMan2::unsetStrut(Window _wid) const
{
    XDeleteProperty(QX11Info::display(), _wid, atom("_NET_WM_STRUT"));
    XDeleteProperty(QX11Info::display(), _wid, atom("_NET_WM_STRUT_PARTIAL"));
}


/************************************************

 ************************************************/
QString XfitMan2::debugWindow(Window wnd)
{
    if (!wnd)
        return QString("[%1]").arg(wnd,8, 16);

    QString typeStr;
    int  format;
    unsigned long type, length, rest, *data;
    length=0;
    if (XGetWindowProperty(QX11Info::display(), wnd, XInternAtom(QX11Info::display(), "_NET_WM_WINDOW_TYPE", False),
                           0, 4096, FALSE, AnyPropertyType, &type, &format,
                           &length, &rest,(unsigned char**) &data) == Success)
    {
        for (unsigned int i = 0; i < length; i++)
        {
            char* aname = XGetAtomName(QX11Info::display(), data[i]);
            typeStr = typeStr + " " + aname;
            XFree(aname);
        }
    }
    else
        typeStr ="ERROR";

    return QString("[%1] %2 %3").arg(wnd,8, 16).arg(xfitMan2().getName(wnd)).arg(typeStr);
}

#endif