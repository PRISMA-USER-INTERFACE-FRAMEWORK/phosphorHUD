#pragma once

inline void EnsurePrismaHUDTrampoline()
{
    static bool s_created = false;
    if (s_created) return;
    REL::GetTrampoline().create(256);
    s_created = true;
    logger::info("PrismaHUD: trampoline created (256 bytes, shared by every hook in this DLL)");
}
