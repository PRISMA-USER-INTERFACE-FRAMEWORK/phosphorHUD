#include "MenuReplacementSuite.h"

#include "BarterMenuController.h"
#include "ContainerMenuController.h"
#include "SleepWaitMenuController.h"

void InitializeMenuReplacementSuite(PRISMA_UI_API::IVPrismaUI11* api)
{
    if (!api) return;

    SleepWaitMenuController::GetSingleton()->Initialize(api);
    BarterMenuController::GetSingleton()->Initialize(api);
    ContainerMenuController::GetSingleton()->Initialize(api);
}
