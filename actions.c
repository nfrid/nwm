#include "actions.h"
#include "bar.h"
#include "client.h"
#include "config.h"
#include "core.h"
#include "monitor.h"
#include "stack.h"
#include "util.h"
#include "variables.h"
#include "xevents.h"

// just quit from wm
void quit(const Arg* arg) { running = 0; }

// set selected tags to arg.ui tagmask
void view(const Arg* arg) {
  int          i;
  unsigned int tmptag;

  // TODO: comment?
  if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK) {
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    selmon->pertag->prevtag         = selmon->pertag->curtag;

    if (arg->ui == ~0)
      selmon->pertag->curtag = 0;
    else {
      for (i = 0; !(arg->ui & 1 << i); i++)
        ;
      selmon->pertag->curtag = i + 1;
    }
  } else {
    tmptag                  = selmon->pertag->prevtag;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag  = tmptag;
  }

  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
  selmon->mfact   = selmon->pertag->mfacts[selmon->pertag->curtag];
  selmon->sellt   = selmon->pertag->sellts[selmon->pertag->curtag];
  selmon->lt[selmon->sellt]
      = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  selmon->lt[selmon->sellt ^ 1]
      = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

  if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
    togglebar(NULL);

  focus(NULL);
  arrange(selmon);
}

// toggle all view
void toggleall(const Arg* arg) {
  Arg tmp;
  if (TAGMASK != selmon->tagset[selmon->seltags]) {
    tmp.ui = TAGMASK;
  } else {
    tmp.ui = selmon->sel->tags;
  }
  view(&tmp);
}

// toggle selected tags with arg.ui tagmask
void toggleview(const Arg* arg) {
  unsigned int newtagset
      = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
  int i;

  // the first visible client should be the same after we add a new tag
  // we also want to be sure not to mutate the focus
  Client* const c = nexttiled(selmon->clients);
  if (c) {
    Client* const selected = selmon->sel;
    pop(c);
    focus(selected);
  }

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;

    if (newtagset == ~0) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      selmon->pertag->curtag  = 0;
    }

    /* test if the user did not select the same tag */
    if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      for (i = 0; !(newtagset & 1 << i); i++)
        ;
      selmon->pertag->curtag = i + 1;
    }

    /* apply settings for this view */
    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact   = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt   = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt]
        = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt ^ 1]
        = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
      togglebar(NULL);

    focus(NULL);
    arrange(selmon);
  }
}

// apply arg.ui tagmask for selected client
void tag(const Arg* arg) {
  if (selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
  }
}

// toggle applied tagmask for selected client for arg.ui tagmask
void toggletag(const Arg* arg) {
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

// set selected tags to arg.ui tagmask for current monitor
void tagmon(const Arg* arg) {
  if (!selmon->sel || !mons->next)
    return;
  Monitor* m = dirtomon(arg->i);
  Client*  c = selmon->sel;
  selmon     = m;
  sendmon(c, m);
  ignorenotify = True;
}

// toggle floating state for selected client
void togglefloating(const Arg* arg) {
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w,
        selmon->sel->h, 0);
  arrange(selmon);
}

// toggle sticky state for selected client
void togglesticky(const Arg* arg) {
  if (!selmon->sel)
    return;
  selmon->sel->issticky = !selmon->sel->issticky;
  arrange(selmon);
}

// toggle fullscreen state for selected client
void togglefullscr(const Arg* arg) {
  if (selmon->sel)
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

Client* geturgent(void) {
  Monitor* m;
  Client*  c = NULL;
  for (m = selmon; m && !(c && c->isurgent); m = m->next)
    for (c = m->clients; c && !c->isurgent; c = c->next)
      ;
  return c;
}

// focus urgent item and its tag
void focusurgent(const Arg* arg) {
  Client* c = geturgent();
  int     i;
  if (c) {
    for (i = 0; i < TAGS_N && !((1 << i) & c->tags); i++)
      ;
    if (i < TAGS_N) {
      const Arg a = { .ui = 1 << i };
      selmon      = c->mon;
      view(&a);
      focus(c);
    }
  }
}

// take urgent item to focused stack
void takeurgent(const Arg* arg) {
  Client* c = geturgent();
  if (c) {
    if (c->mon == selmon) {
      c->tags = selmon->sel->tags;
      focus(c);
      arrange(selmon);
    }
    sendmon(c, selmon);
  }
}

/* move focused item in stack
 * arg.i - shift amount (e.g. +1) */
void movestack(const Arg* arg) {
  Client *c = NULL, *p = NULL, *pc = NULL, *i;

  if (arg->i > 0) {
    /* find the client after selmon->sel */
    for (c = selmon->sel->next; c && (!ISVISIBLE(c) || c->isfloating);
         c = c->next)
      ;
    if (!c)
      for (c = selmon->clients; c && (!ISVISIBLE(c) || c->isfloating);
           c = c->next)
        ;

  } else {
    /* find the client before selmon->sel */
    for (i = selmon->clients; i != selmon->sel; i = i->next)
      if (ISVISIBLE(i) && !i->isfloating)
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i) && !i->isfloating)
          c = i;
  }
  /* find the client before selmon->sel and c */
  for (i = selmon->clients; i && (!p || !pc); i = i->next) {
    if (i->next == selmon->sel)
      p = i;
    if (i->next == c)
      pc = i;
  }

  /* swap c and selmon->sel selmon->clients in the selmon->clients list */
  if (c && c != selmon->sel) {
    Client* temp = selmon->sel->next == c ? selmon->sel : selmon->sel->next;
    selmon->sel->next = c->next == selmon->sel ? c : c->next;
    c->next           = temp;

    if (p && p != c)
      p->next = c;
    if (pc && pc != selmon->sel)
      pc->next = selmon->sel;

    if (selmon->sel == selmon->clients)
      selmon->clients = c;
    else if (c == selmon->clients)
      selmon->clients = selmon->sel;

    arrange(selmon);
  }
}

// cycle through layouts
void cyclelayout(const Arg* arg) {
  Layout* l;
  for (l = (Layout*)layouts; l != selmon->lt[selmon->sellt]; l++)
    ;
  if (arg->i > 0) {
    if (l->symbol && (l + 1)->symbol)
      setlayout(&((Arg) { .v = (l + 1) }));
    else
      setlayout(&((Arg) { .v = layouts }));
  } else {
    if (l != layouts && (l - 1)->symbol)
      setlayout(&((Arg) { .v = (l - 1) }));
    else
      setlayout(&((Arg) { .v = &layouts[layouts_len - 2] }));
  }
}

// resize window by mouse
void resizemouse(const Arg* arg) {
  int      ocx, ocy, nw, nh;
  Client*  c;
  Monitor* m;
  XEvent   ev;
  Time     lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) // don't support resizing fullscreen windows by mouse
    return;
  // TODO: comment
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
          None, cursor[CurResize]->cursor, CurrentTime)
      != GrabSuccess)
    return;
  XWarpPointer(
      dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (c->mon->wx + nw >= selmon->wx
          && c->mon->wx + nw <= selmon->wx + selmon->ww
          && c->mon->wy + nh >= selmon->wy
          && c->mon->wy + nh <= selmon->wy + selmon->wh) {
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
            && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, c->x, c->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(
      dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

// move client by mouse
void movemouse(const Arg* arg) {
  int      x, y, ocx, ocy, nx, ny;
  Client*  c;
  Monitor* m;
  XEvent   ev;
  Time     lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
          None, cursor[CurMove]->cursor, CurrentTime)
      != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (!c->nosnap) {
        if (abs(selmon->wx - nx) < snap)
          nx = selmon->wx;
        else if ((selmon->wx + selmon->ww) - (nx + WIDTH(c)) < snap)
          nx = selmon->wx + selmon->ww - WIDTH(c);
        if (abs(selmon->wy - ny) < snap)
          ny = selmon->wy;
        else if ((selmon->wy + selmon->wh) - (ny + HEIGHT(c)) < snap)
          ny = selmon->wy + selmon->wh - HEIGHT(c);
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
            && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
          togglefloating(NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

// switch bar on/off
void togglebar(const Arg* arg) {
  selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag]
      = !selmon->showbar;
  updatebarpos(selmon);
  resizebarwin(selmon);
  if (showsystray) {
    XWindowChanges wc;
    if (!selmon->showbar)
      wc.y = -barh;
    else if (selmon->showbar) {
      wc.y = 0;
      if (!selmon->topbar)
        wc.y = selmon->mh - barh;
    }
    XConfigureWindow(dpy, systray->win, CWY, &wc);
  }
  arrange(selmon);
}

// cycle through clients' tab modes
void tabmode(const Arg* arg) {
  if (arg && arg->i >= 0)
    selmon->showtab = arg->ui % showtab_nmodes;
  else
    selmon->showtab = (selmon->showtab + 1) % showtab_nmodes;
  arrange(selmon);
}

// switch first client in stack (sel<->first)
void zoom(const Arg* arg) {
  Client* c = selmon->sel;

  if (!selmon->lt[selmon->sellt]->arrange
      || (selmon->sel && selmon->sel->isfloating))
    return;
  if (c == nexttiled(selmon->clients))
    if (!c || !(c = nexttiled(c->next)))
      return;
  pop(c);
}

// set current layout to arg.v
void setlayout(const Arg* arg) {
  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
  if (arg && arg->v)
    selmon->lt[selmon->sellt]
        = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt]
        = (Layout*)arg->v;
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol,
      sizeof selmon->ltsymbol);
  if (selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

// set current ratio factor (arg.f > 1.0 will set it absolutely)
void setmfact(const Arg* arg) {
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
  arrange(selmon);
}

Arg getshifted(const Arg* arg) {
  Arg shifted;

  shifted.ui = selmon->tagset[selmon->seltags];

  if (arg->i > 0) { // left circular shift
    // use only the most left tag
    if (n_ones(shifted.ui) > 1)
      shifted.ui = 1 << (31 - __builtin_clz(shifted.ui));

    shifted.ui = (shifted.ui << arg->i) | (shifted.ui >> (TAGS_N - arg->i));
  } else { // right circular shift
    // use only the most right tag
    if (n_ones(shifted.ui) > 1)
      shifted.ui = 1 << __builtin_ctz(shifted.ui);

    shifted.ui = (shifted.ui >> (-arg->i) | shifted.ui << (TAGS_N + arg->i));
  }

  return shifted;
}

// shift tagmask to next/prev tag
void shiftview(const Arg* arg) {
  Arg shifted = getshifted(arg);
  view(&shifted);
}

// shift tagmask to next/prev tag while tagging the selected client to it
void shifttag(const Arg* arg) {
  Arg shifted = getshifted(arg);
  tag(&shifted);
  view(&shifted);
}

// shift tagmask to next/prev tag from the edgest active (arg.i > 0 is next)
void shiftviewclients(const Arg* arg) {
  Arg          shifted;
  Client*      c;
  unsigned int tagmask = 0;

  for (c = selmon->clients; c; c = c->next)
    tagmask = tagmask | c->tags;

  shifted.ui = selmon->tagset[selmon->seltags];

  if (arg->i > 0) // left circular shift
    do {
      // use only the most left tag
      if (n_ones(shifted.ui) > 1)
        shifted.ui = 1 << (31 - __builtin_clz(shifted.ui));

      shifted.ui = (shifted.ui << arg->i) | (shifted.ui >> (TAGS_N - arg->i));
    } while (tagmask && !(shifted.ui & tagmask));
  else // right circular shift
    do {
      // use only the most right tag
      if (n_ones(shifted.ui) > 1)
        shifted.ui = 1 << __builtin_ctz(shifted.ui);

      shifted.ui = (shifted.ui >> (-arg->i) | shifted.ui << (TAGS_N + arg->i));
    } while (tagmask && !(shifted.ui & tagmask));

  view(&shifted);
}

// change amount of masters (+arg.i)
void incnmaster(const Arg* arg) {
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag]
      = MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

// kill selected client
void killclient(const Arg* arg) {
  if (!selmon->sel)
    return;
  if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask,
          wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

void togglecentering(const Arg* arg) {
  if (arg->i > 0) {
    selmon->sel->nocenter = 0;
  } else if (arg->i < 0) {
    selmon->sel->nocenter = 1;
  } else {
    selmon->sel->nocenter = !selmon->sel->nocenter;
  }
}

void togglesnapping(const Arg* arg) {
  if (arg->i > 0) {
    selmon->sel->nosnap = 0;
  } else if (arg->i < 0) {
    selmon->sel->nosnap = 1;
  } else {
    selmon->sel->nosnap = !selmon->sel->nosnap;
  }
}

// focus a window
void focuswin(const Arg* arg) {
  int     iwin = arg->i;
  Client* c    = NULL;
  for (c = selmon->clients; c && (iwin || !ISVISIBLE(c)); c = c->next) {
    if (ISVISIBLE(c))
      --iwin;
  };
  if (c) {
    focus(c);
    restack(selmon);
  }
}

// focus monitor (arg.i > 0 is next)
void focusmon(const Arg* arg) {
  Monitor* m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
}

// move focus in stack (arg.i > 0 is forward)
void focusstack(const Arg* arg) {
  Client *c = NULL, *i;

  if (!selmon->sel || selmon->sel->isfullscreen)
    return;
  if (arg->i > 0) {
    for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = selmon->clients; i != selmon->sel; i = i->next)
      if (ISVISIBLE(i))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    focus(c);
    restack(selmon);
  }
}
