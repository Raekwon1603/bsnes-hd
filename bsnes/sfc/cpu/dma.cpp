// "HIDE MAIN HUD" second-screen setting (see libretro.cpp's
// smwide_set_hud_hidden and the SETUP tab in the dual-screen fork - see
// super_metroid-android's docs/retroarch-fork-notes.md). Same technique
// that project's own hand-rolled SNES core uses in its src/snes/dma.c
// (see that file's own long comment for the full why): Super Metroid's
// HUD tilemap lives at a fixed WRAM address ($7E:C608, 192 bytes) and is
// DMA'd into VRAM fresh every frame by the ROM's own HandleHudTilemap
// routine - so intercepting the DMA transfer at the moment it copies
// those specific WRAM bytes into the VRAM data port ($2118/$2119) and
// substituting a blank tile lets the WRAM side (and all the game's own
// read-modify-write HUD bookkeeping against it) stay completely
// untouched, while nothing ever reaches the screen. This is cosmetic
// only - it never alters CPU-visible memory state - so it doesn't
// conflict with RetroAchievements hardcore mode the way save states or
// memory patching would.
//
// The static minimap border/frame graphic (InitializeHud, ROM $80:988B,
// 64 bytes) is DMA'd from ROM directly to a fixed VRAM address once per
// room entry, separately from hud_tilemap - filtered here too so it
// doesn't linger on screen after the toggle switches on.
bool g_smwide_hud_hidden = false;
static constexpr uint8_t kHudTilemapWramBank = 0x7e;
static constexpr uint16_t kHudTilemapWramAddr = 0xc608;
static constexpr uint16_t kHudTilemapWramSize = 192;
static constexpr uint8_t kHudMinimapBorderRomBank = 0x80;
static constexpr uint16_t kHudMinimapBorderRomAddr = 0x988b;
static constexpr uint16_t kHudMinimapBorderRomSize = 0x40;
// Fixed VRAM destination for the minimap border/frame DMA above (see
// InitializeHud, sm_80.c: WriteRegWord(VMADDL, addr_unk_605800) right
// before that transfer) - a literal in the ROM's own code, unlike
// hud_tilemap's own VRAM destination which has to be discovered at
// runtime (see g_smwideHudTilemapVramDst below).
static constexpr uint16_t kHudMinimapBorderVramDst = 0x5800;

// Latched the first time transfer() sees hud_tilemap's own DMA go by
// (0xffff = not seen yet this session) - hud_tilemap's VRAM destination
// is stable across frames (same BG3 tilemap base for as long as the
// current room's HUD is up), so this lets smwide_force_blank_hud (called
// from libretro.cpp's smwide_set_hud_hidden) force-blank whatever's
// already sitting in VRAM the instant the toggle flips on, not just gate
// future per-frame writes. Without this, a mostly-static element that's
// only DMA'd once in a while (the minimap border, or hud_tilemap itself
// right after a room's InitializeHud runs, before the toggle was ever
// switched on) sits there stale and visible until something else
// happens to trigger a re-upload - confirmed on real hardware: the
// minimap's row of dots in the corner stayed put through the whole
// Ceres Station opening cutscene even with the toggle already on,
// because InitializeHud's one-time DMA had already happened before the
// player ever reached the second screen's SETUP tab.
static uint16_t g_smwideHudTilemapVramDst = 0xffff;

// Real bug found and fixed after that first attempt still left the dots
// showing: this core runs TWO independent PPU implementations - sfc/ppu
// (cycle-accurate) and sfc/ppu-fast (PPUfast, performance-focused, the
// one HD/widescreen mode's own background-extension code actually lives
// in) - selected at runtime by System::fastPPU() (hacks.fastPPU,
// defaults ON, "recommended to leave active" per this core's own
// options text). sfc/ppu's PPU::load()/power() delegate straight through
// to the global `ppufast` instance whenever fast mode is active (see
// sfc/ppu/ppu.cpp) - and each implementation owns its OWN separate vram
// array and io.vramAddress register copy, not shared. The original
// version of this patch only ever wrote into sfc/ppu's own `ppu.vram`/
// read `ppu`'s own io.vramAddress via forceBlankVramWord/
// vramWriteAddress (see PPU::forceBlankVramWord's own comment,
// sfc/ppu/ppu.hpp) - which is the INACTIVE copy whenever fast mode is on
// (the real default), so those force-blank writes never reached the
// buffer actually being scanned out.
//
// PPUfast's own vram/io fields are public (no private: section follows
// PPUfast's own public: in sfc/ppu-fast/ppu.hpp), but its vramAddress()/
// vramExt() *methods* are declared `alwaysinline` (inline
// __attribute__((always_inline))) with their bodies defined out-of-line
// in ppu-fast/io.cpp and ppu-fast/ppu.cpp - visible only within THAT
// translation unit (ppu-fast/ppu.cpp's own #include chain), not from
// here (cpu.cpp's TU, which dma.cpp is #included into) - calling them
// from here links but leaves them "undefined symbol" at the real
// per-TU-emitted inline body, since always_inline blocks the normal
// external-call fallback plain inline functions get. So this replicates
// PPUfast::vramAddress()'s own real logic (ppu-fast/io.cpp) directly
// against its public io/vram fields instead of calling the method -
// safe, since those are plain data reads, not calls into
// always_inline-only bodies. Simplified for the vramMapping==0 case
// (linear addressing, mode 1-3's interleaved remapping don't apply to
// hud_tilemap's simple sequential DMA - see kHudTilemaps_Row1to3's own
// straight per-word writes, sm_80.c) - vramExt itself
// (configuration.hacks.ppu.mode7.vramExt, plain field, defaults 0x7fff)
// is read directly for the same reason.
auto smwide_hud_tilemap_vram_dst() -> uint {
  if(!system.fastPPU()) return ppu.vramWriteAddress();
  return ppufast.io.vramAddress & configuration.hacks.ppu.mode7.vramExt;
}

// Called from libretro.cpp's smwide_set_hud_hidden the moment the toggle
// switches on - force-blanks whatever's already sitting in VRAM right
// now for both elements transfer() otherwise only gates FUTURE writes
// for (see g_smwideHudTilemapVramDst's own comment above for why this
// needs to exist at all).
auto smwide_force_blank_hud() -> void {
  if(g_smwideHudTilemapVramDst != 0xffff) {
    for(uint w = 0; w < 96; w++) {
      uint addr = (g_smwideHudTilemapVramDst + w) & 0x7fff;
      if(system.fastPPU()) ppufast.vram[addr] = 0x2c0f;
      else ppu.forceBlankVramWord(addr, 0x2c0f);
    }
  }
  //Static minimap border/frame graphic - fixed VRAM destination
  //(kHudMinimapBorderVramDst), unlike hud_tilemap's own dest which has
  //to be discovered at runtime (see above).
  for(uint w = 0; w < kHudMinimapBorderRomSize / 2; w++) {
    uint addr = (kHudMinimapBorderVramDst + w) & 0x7fff;
    if(system.fastPPU()) ppufast.vram[addr] = 0x2c0f;
    else ppu.forceBlankVramWord(addr, 0x2c0f);
  }
}

auto CPU::dmaEnable() -> bool {
  for(auto& channel : channels) if(channel.dmaEnable) return true;
  return false;
}

auto CPU::hdmaEnable() -> bool {
  for(auto& channel : channels) if(channel.hdmaEnable) return true;
  return false;
}

auto CPU::hdmaActive() -> bool {
  for(auto& channel : channels) if(channel.hdmaActive()) return true;
  return false;
}

auto CPU::dmaRun() -> void {
  counter.dma += 8;
  step<8,0>();
  dmaEdge();
  for(auto& channel : channels) channel.dmaRun();
  status.irqLock = true;
}

auto CPU::hdmaReset() -> void {
  for(auto& channel : channels) channel.hdmaReset();
}

auto CPU::hdmaSetup() -> void {
  counter.dma += 8;
  step<8,0>();
  for(auto& channel : channels) channel.hdmaSetup();
  status.irqLock = true;
}

auto CPU::hdmaRun() -> void {
  counter.dma += 8;
  step<8,0>();
  for(auto& channel : channels) channel.hdmaTransfer();
  for(auto& channel : channels) channel.hdmaAdvance();
  status.irqLock = true;
}

//

template<uint Clocks, bool Synchronize>
auto CPU::Channel::step() -> void {
  cpu.counter.dma += Clocks;
  cpu.step<Clocks, Synchronize>();
}

auto CPU::Channel::edge() -> void {
  cpu.dmaEdge();
}

auto CPU::Channel::validA(uint24 address) -> bool {
  //A-bus cannot access the B-bus or CPU I/O registers
  if((address & 0x40ff00) == 0x2100) return false;  //00-3f,80-bf:2100-21ff
  if((address & 0x40fe00) == 0x4000) return false;  //00-3f,80-bf:4000-41ff
  if((address & 0x40ffe0) == 0x4200) return false;  //00-3f,80-bf:4200-421f
  if((address & 0x40ff80) == 0x4300) return false;  //00-3f,80-bf:4300-437f
  return true;
}

auto CPU::Channel::readA(uint24 address) -> uint8 {
  step<4,1>();
  cpu.r.mdr = validA(address) ? bus.read(address, cpu.r.mdr) : (uint8)0x00;
  step<4,1>();
  return cpu.r.mdr;
}

auto CPU::Channel::readB(uint8 address, bool valid) -> uint8 {
  step<4,1>();
  cpu.r.mdr = valid ? bus.read(0x2100 | address, cpu.r.mdr) : (uint8)0x00;
  step<4,1>();
  return cpu.r.mdr;
}

auto CPU::Channel::writeA(uint24 address, uint8 data) -> void {
  if(validA(address)) bus.write(address, data);
}

auto CPU::Channel::writeB(uint8 address, uint8 data, bool valid) -> void {
  if(valid) bus.write(0x2100 | address, data);
}

auto CPU::Channel::transfer(uint24 addressA, uint2 index) -> void {
  uint8 addressB = targetAddress;
  switch(transferMode) {
  case 1: case 5: addressB += index.bit(0); break;
  case 3: case 7: addressB += index.bit(1); break;
  case 4: addressB += index; break;
  }

  //transfers from WRAM to WRAM are invalid
  bool valid = addressB != 0x80 || ((addressA & 0xfe0000) != 0x7e0000 && (addressA & 0x40e000) != 0x0000);

  cpu.r.mar = addressA;
  if(direction == 0) {
    auto data = readA(addressA);
    if(addressB == 0x18 || addressB == 0x19) {
      uint8 bank = addressA >> 16;
      uint16 offset = addressA;
      bool isHudTilemap = bank == kHudTilemapWramBank
          && offset >= kHudTilemapWramAddr && offset < kHudTilemapWramAddr + kHudTilemapWramSize;
      bool isMinimapBorder = bank == kHudMinimapBorderRomBank
          && offset >= kHudMinimapBorderRomAddr && offset < kHudMinimapBorderRomAddr + kHudMinimapBorderRomSize;
      if(isHudTilemap) {
        //Latch this transfer's VRAM destination (pre-increment - the
        //word about to be written lands here) unconditionally, even
        //while the toggle is off, so smwide_force_blank_hud can force-
        //blank it the instant the toggle flips on - see
        //g_smwideHudTilemapVramDst's own comment above. Reads through
        //whichever PPU implementation is actually live (see
        //smwide_hud_tilemap_vram_dst's own comment) - this transfer's
        //own writeB call further down already goes through the real
        //bus dispatch and reaches the right one on its own, but this
        //latch needs to explicitly pick the right io.vramAddress copy.
        g_smwideHudTilemapVramDst = smwide_hud_tilemap_vram_dst() & 0x7ffe;
      }
      if(g_smwide_hud_hidden && isHudTilemap) {
        //blank tile 0x2c0f, same backdrop tile the ROM's own HUD-fill
        //routine uses for every empty HUD position - low byte first,
        //matching the tilemap's little-endian word layout.
        data = ((offset - kHudTilemapWramAddr) & 1) ? 0x2c : 0x0f;
      } else if(g_smwide_hud_hidden && isMinimapBorder) {
        data = ((offset - kHudMinimapBorderRomAddr) & 1) ? 0x2c : 0x0f;
      }
    }
    writeB(addressB, data, valid);
  } else {
    auto data = readB(addressB, valid);
    writeA(addressA, data);
  }
}

auto CPU::Channel::dmaRun() -> void {
  if(!dmaEnable) return;

  step<8,0>();
  edge();

  uint2 index = 0;
  do {
    transfer(sourceBank << 16 | sourceAddress, index++);
    if(!fixedTransfer) !reverseTransfer ? sourceAddress++ : sourceAddress--;
    edge();
  } while(dmaEnable && --transferSize);

  dmaEnable = false;
}

auto CPU::Channel::hdmaActive() -> bool {
  return hdmaEnable && !hdmaCompleted;
}

auto CPU::Channel::hdmaFinished() -> bool {
  auto channel = next;
  while(channel) {
    if(channel->hdmaActive()) return false;
    channel = channel->next;
  }
  return true;
}

auto CPU::Channel::hdmaReset() -> void {
  hdmaCompleted = false;
  hdmaDoTransfer = false;
}

auto CPU::Channel::hdmaSetup() -> void {
  hdmaDoTransfer = true;  //note: needs hardware verification
  if(!hdmaEnable) return;

  dmaEnable = false;  //HDMA will stop active DMA mid-transfer
  hdmaAddress = sourceAddress;
  lineCounter = 0;
  hdmaReload();
}

auto CPU::Channel::hdmaReload() -> void {
  auto data = readA(cpu.r.mar = sourceBank << 16 | hdmaAddress);

  if((uint7)lineCounter == 0) {
    lineCounter = data;
    hdmaAddress++;

    hdmaCompleted = lineCounter == 0;
    hdmaDoTransfer = !hdmaCompleted;

    if(indirect) {
      data = readA(cpu.r.mar = sourceBank << 16 | hdmaAddress++);
      indirectAddress = data << 8 | 0x00;  //todo: should 0x00 be indirectAddress >> 8 ?
      if(hdmaCompleted && hdmaFinished()) return;

      data = readA(cpu.r.mar = sourceBank << 16 | hdmaAddress++);
      indirectAddress = data << 8 | indirectAddress >> 8;
    }
  }
}

auto CPU::Channel::hdmaTransfer() -> void {
  if(!hdmaActive()) return;
  dmaEnable = false;  //HDMA will stop active DMA mid-transfer
  if(!hdmaDoTransfer) return;

  static const uint lengths[8] = {1, 2, 2, 4, 4, 4, 2, 4};
  for(uint2 index : range(lengths[transferMode])) {
    uint24 address = !indirect ? sourceBank << 16 | hdmaAddress++ : indirectBank << 16 | indirectAddress++;
    transfer(address, index);
  }
}

auto CPU::Channel::hdmaAdvance() -> void {
  if(!hdmaActive()) return;
  lineCounter--;
  hdmaDoTransfer = bool(lineCounter & 0x80);
  hdmaReload();
}
