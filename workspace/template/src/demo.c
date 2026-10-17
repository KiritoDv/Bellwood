#include "mod.h"

#include "port/events/Events.h"
#include "port/api/cvar.h"
#include "port/api/ui.h"
#include "m_common_data.h"
#include "m_private.h"
#include <stdio.h>

#define CVAR_FULL_WALLET "gMods.Demo.FullWallet"

static ListenerID sPlayUpdateListenerID;
static ListenerID sMoneyChangedListenerID;

static void SetupUI(void) {
    C_WidgetConfig chk = { 0 };
    chk.type = C_WIDGET_CVAR_CHECKBOX;
    chk.cvar = CVAR_FULL_WALLET;
    chk.opts.checkbox.tooltip = "Keeps your wallet full of Bells.";
    C_AddSidebarEntry("Demo Mod", 1);
    C_AddWidget("Demo Mod", 1, "Full Wallet", &chk);
}

static void OnPlayUpdate(IEvent* event) {
    Private_c* priv = Common_Get(now_private);

    if (CVarGetInteger(CVAR_FULL_WALLET, 0) == 0 || priv == NULL) {
        return;
    }

    priv->inventory.wallet = mPr_WALLET_MAX;
}

static void OnMoneyChanged(IEvent* event) {
    PlayerMoneyChanged* money = (PlayerMoneyChanged*)event;

    printf("[Demo Mod] Wallet: %u -> %u Bells\n", money->prev, money->now);
}

MOD_INIT() {
    SetupUI();
    sPlayUpdateListenerID = REGISTER_LISTENER(PlayUpdate, EVENT_PRIORITY_NORMAL, OnPlayUpdate);
    sMoneyChangedListenerID = REGISTER_LISTENER(PlayerMoneyChanged, EVENT_PRIORITY_NORMAL, OnMoneyChanged);
}

MOD_EXIT() {
    C_RemoveSidebarEntry("Demo Mod");

    UNREGISTER_LISTENER(PlayUpdate, sPlayUpdateListenerID);
    UNREGISTER_LISTENER(PlayerMoneyChanged, sMoneyChangedListenerID);
}
