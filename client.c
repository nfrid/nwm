#include "bar.h"
#include "client.h"
#include "config.h"
#include "core.h"
#include "layouts.h"
#include "monitor.h"
#include "stack.h"
#include "util.h"
#include "variables.h"
#include "xevents.h"

// configure client by XConfigureEvent
void configure(Client* c) {
  XConfigureEvent ce;

  ce.type              = ConfigureNotify;
  ce.display           = dpy;
  ce.event             = c->win;
  ce.window            = c->win;
  ce.x                 = c->x;
  ce.y                 = c->y;
  ce.width             = c->w;
  ce.height            = c->h;
  ce.border_width      = c->bw;
  ce.above             = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent*)&ce);
}

// focus some client
void focus(Client* c) {
  // TODO: comment
  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (selmon->sel && selmon->sel != c)
    unfocus(selmon->sel, 0);
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  drawbars();
  drawtabs();
}

// resize client (wrapper w/ sizehints)
void resize(Client* c, int x, int y, unsigned w, unsigned h, Bool interact) {
  if (applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

// actual resize client
void resizeclient(Client* c, int x, int y, unsigned w, unsigned h) {
  XWindowChanges wc;
  unsigned       n;
  unsigned       gapoffset;
  unsigned       gapincr;
  Client*        nbc;

  wc.border_width = c->bw;

  /* Get number of clients for the client's monitor */
  for (n = 0, nbc = nexttiled(c->mon->clients); nbc;
       nbc = nexttiled(nbc->next), n++)
    ;

  /* Do nothing if layout is floating */
  if (c->isfloating || c->mon->lt[c->mon->sellt]->arrange == NULL) {
    gapincr = gapoffset = 0;
  } else {
    /* Remove border and gap if layout is monocle or only one client */
    if (c->mon->lt[c->mon->sellt]->arrange == monocle || n == 1) {
      gapoffset       = 0;
      gapincr         = -2 * borderpx;
      wc.border_width = 0;
    } else {
      gapoffset = gappx;
      gapincr   = 2 * gappx;
    }
  }

  c->oldx = c->x;
  c->x = wc.x = x + gapoffset;
  c->oldy     = c->y;
  c->y = wc.y = y + gapoffset;
  c->oldw     = c->w;
  c->w = wc.width = w - gapincr;
  c->oldh         = c->h;
  c->h = wc.height = h - gapincr;

  XConfigureWindow(
      dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(c);
  XSync(dpy, False);
}

// send client to monitor
void sendmon(Client* c, Monitor* m) {
  if (c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon  = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attach(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

// set state of a client
void setclientstate(Client* c, long state) {
  long data[] = { state, None };

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
      PropModeReplace, (unsigned char*)data, 2);
}

// set client as focused
void setfocus(Client* c) {
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
        PropModeReplace, (unsigned char*)&(c->win), 1);
  }
  sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus],
      CurrentTime, 0, 0, 0);
}

// set client as fullscreen
void setfullscreen(Client* c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate     = c->isfloating;
    c->oldbw        = c->bw;
    c->bw           = 0;
    c->isfloating   = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)0, 0);
    c->isfullscreen = 0;
    c->isfloating   = c->oldstate;
    c->bw           = c->oldbw;
    c->x            = c->oldx;
    c->y            = c->oldy;
    c->w            = c->oldw;
    c->h            = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

// set client as urgent
void seturgent(Client* c, int urg) {
  XWMHints* wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

// show/hide a client
void showhide(Client* c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating)
        && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

// unfocus a client
void unfocus(Client* c, int setfocus) {
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

// unmanage a client
void unmanage(Client* c, int destroyed) {
  Monitor*       m = c->mon;
  XWindowChanges wc;

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}

// update size hints 🗿
void updatesizehints(Client* c) {
  long       msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

// resize client using size hints (sticky stuff)
Bool applysizehints(
    Client* c, int* x, int* y, unsigned* w, unsigned* h, Bool interact) {
  Bool     baseismin;
  Monitor* m = c->mon;

  // set minimum possible
  *w = MAX(1, *w);
  *h = MAX(1, *h);

  if (interact) {
    if (*x > screenw)
      *x = screenw - WIDTH(c);
    if (*y > screenh)
      *y = screenh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < barh)
    *h = barh;
  if (*w < barh)
    *w = barh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

// update client's title
void updatetitle(Client* c) {
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

// update client's window type
void updatewindowtype(Client* c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if (state == netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

// update client's wm hints
void updatewmhints(Client* c) {
  XWMHints* wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

// update client's decoration hints (motif)
void updatemotifhints(Client* c) {
  Atom           real;
  int            format;
  unsigned char* p = NULL;
  unsigned long  n, extra;
  unsigned long* motif;
  int            width, height;

  if (!decorhints)
    return;

  if (XGetWindowProperty(dpy, c->win, motifatom, 0L, 5L, False, motifatom,
          &real, &format, &n, &extra, &p)
          == Success
      && p != NULL) {
    motif = (unsigned long*)p;
    if (motif[MWM_HINTS_FLAGS_FIELD] & MWM_HINTS_DECORATIONS) {
      width  = WIDTH(c);
      height = HEIGHT(c);

      if (motif[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_ALL
          || motif[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_BORDER
          || motif[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_TITLE)
        c->bw = c->oldbw = borderpx;
      else
        c->bw = c->oldbw = 0;

      resize(c, c->x, c->y, width - (2 * c->bw), height - (2 * c->bw), 0);
    }
    XFree(p);
  }
}

// apply rules based on config
void applyrules(Client* c) {
  const char* class, *instance;
  unsigned int i;
  const Rule*  r;
  Monitor*     m;
  XClassHint   ch = { NULL, NULL };

  c->isfloating = 0;
  c->tags       = 0;
  XGetClassHint(dpy, c->win, &ch);
  class    = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  // rule matching & applying
  for (i = 0; i < rules_len; i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title))
        && (!r->class || strstr(class, r->class))
        && (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->nosnap     = r->nosnap;
      c->tags |= r->tags;
      for (m = mons; m && m->num != r->monitor; m = m->next)
        ;
      if (m)
        c->mon = m;
    }
  }

  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);

  // apply tagmask if can
  c->tags
      = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}
