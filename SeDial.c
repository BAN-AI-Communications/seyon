/*
 * This file is part of the Seyon, Copyright (c) 1992-1993 by Muhammad M.
 * Saggaf. All rights reserved.
 *
 * See the file COPYING (1-COPYING) or the manual page seyon(1) for a full
 * statement of rights and permissions for this program.
 */

#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "MultiList.h"
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Viewport.h>

#include "SeDecl.h"
#include "SeSig.h"
#include "seyon.h"

extern char *get_word();
extern int IconifyShell(), MdmReadLine(), MdmTimedWaitFor();
extern int ConvertStringToIntArray();

int ReadParsePhoneFile();
void DoDial(), ExecDial(), DismissDirectory(), DialerEnd(), HighlightItems(),
    ClearAllItems(), ManualDial(), DoManualDial(), ReReadPhoneFile();

struct _ddItem {
  char number[LIT_BUF];
  char name[LIT_BUF];
  char baud[10];
  int bits;
  int parity;
  int stopBits;
  char prefix[LIT_BUF];
  char suffix[LIT_BUF];
  char script[SM_BUF];
};

struct _ddItem *ddItems[MAX_ENT];
Boolean dialDirUp = False, manualDial = False;
char *script_file, phone_number[SM_BUF] = "", dialMsg[SM_BUF];
int dialTime, dialTry, current_item, ddCurItemIndex;
jmp_buf dial_env;
XfwfMultiListWidget mlw;

#define BUGGY_MULTILIST_WIDGET 1

/*---------------------------------------------------------------------------+
| TopDial - the top routine for the dialing directory.
+---------------------------------------------------------------------------*/

void TopDial(widget, clientData) Widget widget;
XtPointer clientData;
{
  void EditFile();

  static Widget popup, view, list;
  Widget form, mBox, uBox, lBox;
  static char phoneFile[REG_BUF];
  static String disItems[MAX_ENT + 1] = {NULL};

  if (clientData == NULL && dialDirUp) {
    XMapRaised(XtDisplay(widget), XtWindow(popup));
    return;
  }

  if (disItems[0] == NULL) {
    strncpy(phoneFile, qres.phoneFile, REG_BUF);
    if (ReadParsePhoneFile(phoneFile, disItems) < 0)
      return;

    form = XtParent(widget);
    popup = SeAddPopupWG("directory", widget, form, form, 0,
                         SeWidgetHeight(form), True, True);
    mBox = AddPaned("mBox", popup);
    /*      uBox = AddBox("uBox", mBox);*/
    uBox = SeAddForm("uBox", mBox);
    lBox = AddBox("lBox", mBox);

    view = XtCreateManagedWidget("view", viewportWidgetClass, uBox, NULL, 0);
    list = XtVaCreateManagedWidget("list", xfwfMultiListWidgetClass, view,
                                   XtNlist, disItems, NULL);
    mlw = (XfwfMultiListWidget)list;
    SeSetViewportDimFromMultiList(view, list, 10);
    XtAddCallback(list, XtNcallback, DoDial, NULL);

    AddButton("dismiss", lBox, DismissDirectory, NULL);
    AddButton("ok", lBox, DoDial, NULL);
    AddButton("manual", lBox, ManualDial, NULL);
    AddButton("clear", lBox, ClearAllItems, NULL);
    AddButton("default", lBox, HighlightItems, qres.defaultPhoneEntries);
    AddButton("edit", lBox, EditFile, (XtPointer)phoneFile);
    AddButton("reread", lBox, ReReadPhoneFile, (XtPointer)disItems);

    XtRealizeWidget(popup);
  }

#if BUGGY_MULTILIST_WIDGET
  else {
    XtDestroyWidget(list);

    list = XtVaCreateManagedWidget("list", xfwfMultiListWidgetClass, view,
                                   XtNlist, disItems, NULL);
    mlw = (XfwfMultiListWidget)list;
    /*      SeSetViewportDimFromMultiList(view, list, 10);*/
    XtAddCallback(list, XtNcallback, DoDial, NULL);

    XtRealizeWidget(popup);
  }
#endif

  if (clientData == NULL) {
    XtPopup(popup, XtGrabNone);
    dialDirUp = True;
    if (qres.dialAutoStart) {
      qres.dialAutoStart = False;
      HighlightItems(widget, qres.defaultPhoneEntries);
      DoDial();
    }
  } else {
    HighlightItems(widget, clientData);
    DoDial();
  }
}

/*
 * DoDial: gets the first selected item and dials it.
 */

void DoDial() {
  static XfwfMultiListReturnStruct *item;

  ErrorIfBusy();

  if ((item = XfwfMultiListGetHighlighted(mlw))->num_selected == 0)
    SimpleError("No Item Selected");
  ddCurItemIndex = item->selected_items[(current_item = 0)];

  inhibit_child = True;
  manualDial = False;
  dialTry = 1;

  PreProcessPrep();
  ExecDial();
}

/*
 * executes the dialing as a forked process and communicates progress
 * to the main process
 */

void DialHandler(
#if NeedFunctionPrototypes
    int signo, XtPointer client_data
#endif
) {
  void DispatchActions(), RunScript();

#if defined(SUNOS_3) || defined(Mips)
  union wait status;
#else
  int status;
#endif
  Widget dirWidget;

  if (wait(&status) < 0) {
    SePError("Dial wait failed");
    return;
  }
  XoAppIgnoreSignal(app_con, SIGCHLD);

#if defined(SUNOS_3) || defined(Mips)
  switch (status.w_retcode) {
#else
  switch (WEXITSTATUS(status)) {
#endif
  case 0:
    if (manualDial)
      SeyonMessage("Connected");
    else
      SeyonMessage(
          FmtString1("Connected to %s", ddItems[ddCurItemIndex]->name));
    UpdateStatusBox(NULL);
    DispatchActions(ACTION_DISPATCH, qres.postConnectAction, genericWidget);

    if ((dirWidget = XtNameToWidget(dialWidget, "directory"))) {
      RemoveCurrentItem();
      if (ddItems[ddCurItemIndex]->script[0] && !manualDial) {
        linkflag = 1;
        inhibit_child = False;
        RunScript(NULL, ddItems[ddCurItemIndex]->script);
        return;
      }
    }
    break;
  case 1:
    DialCirculate();
    return;
  case 2:
    Beep();
    SeyonMessage("Dial Aborted: Online");
    break;
  case 10:
    SeyonMessage("Dialing Canceled by User");
    break;
  }

  inhibit_child = False;
  PostProcessPrep();
}

/*
 * KillDialHandler: signal handler for canceling the dialing process.
 */

void KillDialHandler(
#if NeedFunctionPrototypes
    int signo
#endif
) {
  void MdmOFlush();

  signal(SIGTERM, SIG_IGN);
  MdmOFlush();
  cancel_dial(0);
  exit(10);
}

void ExecDial() {
  int DialNumber();
  static int retStatus;

  XoAppAddSignal(app_con, SIGCHLD, DialHandler, NULL);

  if ((w_child_pid = SeFork()))
    return;

  /* Child process */
  signal(SIGTERM, KillDialHandler);
  retStatus = DialNumber();
  exit(retStatus);
}

/*
 * ReReadPhoneFile: updates the dialing directory from the phonebook file.
 */

void ReReadPhoneFile(widget, disItems) Widget widget;
XtPointer disItems[];
{
  Widget dirWidget = XtParent(GetShell(widget));

  ErrorIfBusy();

  FreeList(disItems);
  DestroyShell(widget);
  dialDirUp = False;
  TopDial(dirWidget, NULL);
}

/*
 * ManualDial: sets up the manual dialing popup.
 */

void ManualDial(widget) Widget widget;
{
  Widget popup;

  ErrorIfBusy();
  popup = GetShell(PopupDialogGetValue("manual_dial", widget, DoManualDial,
                                       NULL, phone_number));
  PopupCentered(popup, widget);
}

/*
 * action proc for manual dialing
 */

void manual_dial_action_ok(widget) Widget widget;
{ DoManualDial(widget); }

/*
 * does manual dialing
 */

void DoManualDial(widget) Widget widget;
{
  void ExecManualDial();
  Widget dialog = XtParent(widget);
  char phoneNumber[SM_BUF];

  strncpy(phoneNumber, XawDialogGetValueString(dialog), SM_BUF);
  DestroyShell(dialog);
  ExecManualDial(XtParent(GetShell(widget)), phoneNumber);
}

void ExecManualDial(widget, phoneNumber) Widget widget;
String phoneNumber;
{
  inhibit_child = True;
  manualDial = True;

  strncpy(phone_number, phoneNumber, SM_BUF);

  dialTry = 1;
  PreProcessPrep();
  ExecDial();
}

/*
 * DismissDirectory: dismisses the dialing directory.
 */

void DismissDirectory(widget, clientData) Widget widget;
XtPointer clientData;
{
  dialDirUp = False;
  XtPopdown(GetShell(widget));
}

/*
 * ClearAllItems: clears all selected items.
 */

void ClearAllItems() { XfwfMultiListUnhighlightAll(mlw); }

void HighlightItems(widget, clientData) Widget widget;
XtPointer clientData;
{
  int dialEntries[MAX_ENT], i;

  ConvertStringToIntArray((String *)clientData, dialEntries);

  ClearAllItems();
  for (i = 0; dialEntries[i]; i++)
    XfwfMultiListHighlightItem(mlw, dialEntries[i] - 1);
}

/*
 * unhighlights (unselects) the current item
 */

void RemoveCurrentItem() {
  static XfwfMultiListReturnStruct *item;

  item = XfwfMultiListGetHighlighted(mlw);
  XfwfMultiListUnhighlightItem(mlw, item->selected_items[current_item]);
}

/*
 * circulates to the next selected item and dials it
 */

void DialCirculate() {
  static XfwfMultiListReturnStruct *item;

  if (manualDial)
    dialTry++;
  else {
    item = XfwfMultiListGetHighlighted(mlw);
    if (++current_item == item->num_selected) {
      current_item = 0;
      dialTry++;
    }
    ddCurItemIndex = item->selected_items[current_item];
  }

  if (qres.dialRepeat && dialTry > qres.dialRepeat) {
    SeyonMessage("Max Tries Exhausted");
    inhibit_child = False;
    PostProcessPrep();
    return;
  }

  ExecDial();
}

void GetStrField(raw, keyword, var, def) String raw, keyword, var, def;
{
  char *ptr, buf[REG_BUF], wrd[REG_BUF];

  if ((ptr = (char *)strstr(raw, keyword)) != NULL) {
    ptr += strlen(keyword);
    if (strncmp(ptr, "CURRENT", 3)) {
      strncpy(buf, ptr, REG_BUF);
      GetWord(buf, wrd);
      strncpy(var, wrd, sizeof(*var));
    } else
      strncpy(var, "CURRENT", sizeof(*var));
  } else
    strncpy(var, def, sizeof(*var));
}

void GetIField(raw, keyword, var, def) String raw, keyword;
int *var, def;
{
  char svar[TIN_BUF], sdef[TIN_BUF];

  sprintf(svar, "%d", *var); /* safe */
  sprintf(sdef, "%d", def);  /* safe */

  GetStrField(raw, keyword, svar, sdef);

  if (strcmp(svar, "CURRENT"))
    *var = atoi(svar);
  else
    *var = 100;
}

#define DIALALARM 5

/*
 * DialTimerHandler: alarm handler for dial timeout.
 */

void DialTimerHandler(dummy) int dummy;
{
  void DialTimerHandler();

  ProcRequest(SET_MESSAGE,
              FmtString("%s... %d", dialMsg, dialTime -= DIALALARM, ""), "");

  if (dialTime > 0) {
    signal(SIGALRM, DialTimerHandler);
    alarm(DIALALARM);
  } else {
    signal(SIGALRM, SIG_DFL);
    alarm(0);
    strncpy(dialMsg, "TIMEOUT", SM_BUF);
    longjmp(dial_env, 1);
  }
}

/*
 * DialNumber: this routine actually does the dialing.
 */

int DialNumber() {
  void MdmIFlush();
  char modemResponse[SM_BUF], *itemName, *bufPtr, dialString[REG_BUF];
  int i,
      /*                  length, */
      length_remaining, k;

  if (setjmp(dial_env) != 0) {
    signal(SIGALRM, SIG_DFL);
    alarm(0);

    ProcRequest(SET_MESSAGE,
                FmtString("%s: Sleeping (%ds)...", dialMsg, qres.dialDelay, ""),
                "");
    cancel_dial(0);
    sleep(qres.dialDelay);

    return 1;
  }

  k = ddCurItemIndex;
  dialTime = qres.dialTimeOut;

  if (!manualDial) {

    itemName = ddItems[k]->name;
    strncpy(dialString,
            FmtString("\r%s %s%s", ddItems[k]->prefix, ddItems[k]->number,
                      ddItems[k]->suffix),
            REG_BUF);

    if (mbaud(ddItems[k]->baud) < 0)
      se_warningf("invalid BPS value in dialing directory: %s",
                  ddItems[k]->baud, "", "");
    if (MdmSetGetCSize(ddItems[k]->bits) < 0)
      se_warningf("invalid BITS value in dialing directory: %d",
                  ddItems[k]->bits, "", "");
    if (MdmSetGetParity(ddItems[k]->parity) < 0)
      se_warningf("invalid PARITY value in dialing directory: %d",
                  ddItems[k]->parity, "", "");
    if (MdmSetGetStopBits(ddItems[k]->stopBits) < 0)
      se_warningf("invalid STOPB value in dialing directory: %d",
                  ddItems[k]->stopBits, "", "");
  } else {
    itemName = phone_number;
    strncpy(
        dialString,
        FmtString("\r%s %s%s", qres.dialPrefix, phone_number, qres.dialSuffix),
        REG_BUF);
  }

  length_remaining = SM_BUF;
  if (dialTry == 1) {
    strncpy(dialMsg, "Dialing ", length_remaining);
    length_remaining -= strlen("Dialing ");
    strncat(dialMsg, itemName, length_remaining);
  } else
    strncpy(dialMsg, "Redialing ", length_remaining);
  length_remaining -= strlen("Redialing ");
  sprintf(dialMsg, "%1.1d ", dialTry);
  length_remaining -= 2;
  strncat(dialMsg, itemName, length_remaining);

  ProcRequest(SET_MESSAGE, "Setting Up...", "");

  if (qres.hangupBeforeDial && (qres.ignoreModemDCD || Online()))
    MdmHangup();
  if (!qres.ignoreModemDCD && Online())
    return 2;

  ProcRequest(SET_MESSAGE, FmtString("%s... %d", dialMsg, dialTime, ""), "");

  MdmIFlush();
  MdmPutString(dialString);
  MdmPurge();

  signal(SIGALRM, DialTimerHandler);
  alarm(DIALALARM);

  while (1) {
    MdmReadLine(modemResponse);

    if (*(bufPtr = StripSpace(qres.connectString)) &&
        strncmp(modemResponse, bufPtr, strlen(bufPtr)) == 0) {
      alarm(0);
      signal(SIGALRM, SIG_DFL);
      showf("\r\n%s", modemResponse, "", "");
      return 0;
    }

    for (i = 0; i < 3; i++)
      if (*(bufPtr = StripSpace(qres.noConnectString[i])) &&
          strncmp(modemResponse, bufPtr, strlen(bufPtr)) == 0) {
        strncpy(dialMsg, modemResponse, SM_BUF);
        longjmp(dial_env, 1);
      }
  } /* while(1)... */
}

int ReadParsePhoneFile(fname, disItems) String fname;
String disItems[];
{
  FILE *fp;
  FILE *devnull;
  String rawItems[MAX_ENT + 1];
  char *buf, *sHold, disItemsBuf[REG_BUF];
  char filename[LRG_BUF];
  int i, n, length, iHold;

  strncpy(filename, fname, REG_BUF);

  if ((fp = open_file(filename, REG_BUF, qres.defaultDirectory)) == NULL)
    return -1;

  ReadCommentedFile(fp, rawItems);
  fclose(fp);

  FreeList(ddItems);
  for (i = 0; (buf = rawItems[i]); i++) {

    /* Allocate the record */
    ddItems[i] = XtNew(struct _ddItem);

    /* Find the number */
    GetWord(buf, ddItems[i]->number);

    /* Find the name */
    GetWord((buf = lptr), ddItems[i]->name);

    /* Find other stuff */
    GetStrField(buf, "BPS=", ddItems[i]->baud, qres.defaultBPS);
    GetIField(buf, "BITS=", &(ddItems[i]->bits), qres.defaultBits);
    GetIField(buf, "PARITY=", &(ddItems[i]->parity), qres.defaultParity);
    GetIField(buf, "STOPB=", &(ddItems[i]->stopBits), qres.defaultStopBits);
    GetStrField(buf, "PREFIX=", ddItems[i]->prefix, qres.dialPrefix);
    GetStrField(buf, "SUFFIX=", ddItems[i]->suffix, qres.dialSuffix);
    GetStrField(buf, "SCRIPT=", ddItems[i]->script, "\000");
  }
  ddItems[i] = (struct _ddItem *)NULL;

  FreeList(rawItems);
  FreeList(disItems);

  /* Ick... This is horrible - using a user-provided format string
     means we have no easy way of limiting string length. HACK HACK
     HACK Use fprintf to output to /dev/null and count the number of
     bytes... It would be nice if we could rely on having snprintf() */

  devnull = fopen("/dev/null", "r+");
  if (NULL == devnull) {
    printf("Open /dev/null failed!?!\n");
    return 1;
  }

  for (n = 0; n < i; n++) {
    length = fprintf(
        devnull, qres.dialDirFormat, ddItems[n]->name, ddItems[n]->number,
        strncmp((sHold = ddItems[n]->baud), "CUR", 3) ? sHold : "????",
        (iHold = ddItems[n]->bits) == 100 ? '?' : itoa(iHold),
        (iHold = ddItems[n]->parity)
            ? (iHold == 1 ? 'O' : (iHold == 2 ? 'E' : '?'))
            : 'N',
        (iHold = ddItems[n]->stopBits) == 100 ? '?' : itoa(iHold),
        strncmp((sHold = ddItems[n]->prefix), "CUR", 3)
            ? strcmp(sHold, qres.dialPrefix) ? 'P' : 'D'
            : '?',
        strncmp((sHold = ddItems[n]->suffix), "CUR", 3)
            ? strcmp(sHold, qres.dialSuffix) ? 'S' : 'D'
            : '?',
        ddItems[n]->script);

    if (REG_BUF >= length) {
      sprintf(disItemsBuf, qres.dialDirFormat, ddItems[n]->name,
              ddItems[n]->number,
              strncmp((sHold = ddItems[n]->baud), "CUR", 3) ? sHold : "????",
              (iHold = ddItems[n]->bits) == 100 ? '?' : itoa(iHold),
              (iHold = ddItems[n]->parity)
                  ? (iHold == 1 ? 'O' : (iHold == 2 ? 'E' : '?'))
                  : 'N',
              (iHold = ddItems[n]->stopBits) == 100 ? '?' : itoa(iHold),
              strncmp((sHold = ddItems[n]->prefix), "CUR", 3)
                  ? strcmp(sHold, qres.dialPrefix) ? 'P' : 'D'
                  : '?',
              strncmp((sHold = ddItems[n]->suffix), "CUR", 3)
                  ? strcmp(sHold, qres.dialSuffix) ? 'S' : 'D'
                  : '?',
              ddItems[n]->script);
      disItemsBuf[SM_BUF - 1] = '\0';
      disItems[n] = XtNewString(disItemsBuf);
    } else {
      printf("ReadParsePhoneFile: attempted overrun: %s\n", ddItems[n]->name);
      fclose(devnull);
      return 1;
    }
  }
  disItems[n] = NULL;
  fclose(devnull);

  return 0;
}
