#define INIT_EVENT_IDS
#include "Events.h"

extern "C" void PortEvents_Init(void) {
    REGISTER_EVENT(EngineReady);
    REGISTER_EVENT(KeyboardInput);
    REGISTER_EVENT(GameLoopTick);
    REGISTER_EVENT(GameFrameUpdate);
    REGISTER_EVENT(GameStateChanged);
    REGISTER_EVENT(GameReset);
    REGISTER_EVENT(ControllerRead);

    REGISTER_EVENT(ActorProfileLookup);
    REGISTER_EVENT(NpcProfileLookup);
    REGISTER_EVENT(MaskNpcLookup);
    REGISTER_EVENT(CustomNpcTalk);
    REGISTER_EVENT(ActorSpawn);
    REGISTER_EVENT(ActorInit);
    REGISTER_EVENT(ActorUpdate);
    REGISTER_EVENT(ActorDraw);
    REGISTER_EVENT(ActorDestroy);

    REGISTER_EVENT(FurnitureLookup);

    REGISTER_EVENT(ItemLookup);
    REGISTER_EVENT(ItemCollected);
    REGISTER_EVENT(ItemUse);
    REGISTER_EVENT(MuseumDonatorLookup);
    REGISTER_EVENT(MuseumDonate);
    REGISTER_EVENT(MuseumDonatorRemoved);

    REGISTER_EVENT(InteriorLookup);
    REGISTER_EVENT(CreatureLookup);
    REGISTER_EVENT(FishSpawnList);
    REGISTER_EVENT(InsectSpawnList);
    REGISTER_EVENT(ClothLookup);
    REGISTER_EVENT(VillagerLookup);
    REGISTER_EVENT(VillagerMovedIn);
    REGISTER_EVENT(VillagerMovedOut);
    REGISTER_EVENT(NpcTalkRequest);
    REGISTER_EVENT(NpcTalkStart);
    REGISTER_EVENT(StructureLookup);

    REGISTER_EVENT(ShopStockGenerated);
    REGISTER_EVENT(CatalogPageBuild);

    REGISTER_EVENT(PlayerStateRequest);
    REGISTER_EVENT(PlayerStateChanged);
    REGISTER_EVENT(PlayerMoneyChanged);
    REGISTER_EVENT(PlayerInventoryChanged);
    REGISTER_EVENT(PlayerAcreChanged);
    REGISTER_EVENT(FishCaught);
    REGISTER_EVENT(InsectCaught);
    REGISTER_EVENT(PlayerDig);
    REGISTER_EVENT(PlayerShakeTree);
    REGISTER_EVENT(LetterReceived);
    REGISTER_EVENT(LoanChanged);
    REGISTER_EVENT(HouseSizeChanged);

    REGISTER_EVENT(SceneChangeRequest);
    REGISTER_EVENT(SceneLoaded);
    REGISTER_EVENT(SceneUnloaded);
    REGISTER_EVENT(PlayUpdate);
    REGISTER_EVENT(FamicomStart);
    REGISTER_EVENT(FamicomRomLookup);
    REGISTER_EVENT(NewGameWelcome);
    REGISTER_EVENT(TrainRideStart);
    REGISTER_EVENT(MessageLoad);
    REGISTER_EVENT(MessageLookup);
    REGISTER_EVENT(MessageLoaded);
    REGISTER_EVENT(MessageOpened);
    REGISTER_EVENT(MessageClosed);
    REGISTER_EVENT(ChoiceSelected);
    REGISTER_EVENT(ChoiceStringLookup);
    REGISTER_EVENT(SubmenuOpen);
    REGISTER_EVENT(SubmenuOpened);
    REGISTER_EVENT(SubmenuClosed);
    REGISTER_EVENT(TimeHourChanged);
    REGISTER_EVENT(TimeDayChanged);
    REGISTER_EVENT(WeatherChanged);

    REGISTER_EVENT(SaveLoaded);
    REGISTER_EVENT(SaveRequest);
    REGISTER_EVENT(SaveWritten);
    REGISTER_EVENT(SaveImported);

    REGISTER_EVENT(FieldItemChanged);
    REGISTER_EVENT(ClockRead);

    REGISTER_EVENT(SatellaConnected);
    REGISTER_EVENT(SatellaDisconnected);
    REGISTER_EVENT(ResetDetected);

    REGISTER_EVENT(PlayBgmEvent);
    REGISTER_EVENT(StopBgmEvent);
    REGISTER_EVENT(PlaySystemSfxEvent);
    REGISTER_EVENT(PlaySfxEvent);

    REGISTER_EVENT(RenderGamePre);
    REGISTER_EVENT(RenderGamePost);
}
