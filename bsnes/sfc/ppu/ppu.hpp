struct PPU : Thread, PPUcounter {
  alwaysinline auto interlace() const -> bool { return display.interlace; }
  alwaysinline auto overscan() const -> bool { return display.overscan; }
  alwaysinline auto vdisp() const -> uint { return display.vdisp; }

  //ppu.cpp
  PPU();
  ~PPU();

  auto synchronizeCPU() -> void;
  static auto Enter() -> void;
  auto load() -> bool;
  auto power(bool reset) -> void;

  //main.cpp
  auto main() -> void;
  noinline auto cycleObjectEvaluate() -> void;
  template<uint Cycle> noinline auto cycleBackgroundFetch() -> void;
  noinline auto cycleBackgroundBegin() -> void;
  noinline auto cycleBackgroundBelow() -> void;
  noinline auto cycleBackgroundAbove() -> void;
  noinline auto cycleRenderPixel() -> void;
  template<uint> auto cycle() -> void;

  //io.cpp
  auto latchCounters(uint hcounter, uint vcounter) -> void;
  auto latchCounters() -> void;

  //serialization.cpp
  auto serialize(serializer&) -> void;

  // Same reasoning as forceBlankVramWord below (defined out-of-line,
  // after this class closes - see that function's own comment for why
  // either of these needs to exist at all) - io.vramAddress (VMADDL/
  // VMADDH, $2116/$2117) is also private, and CPU::Channel::transfer
  // needs to read it to latch hud_tilemap's own DMA destination (see
  // g_smwideHudTilemapVramDst in dma.cpp).
  alwaysinline auto vramWriteAddress() const -> uint16 { return io.vramAddress; }

  // Backs the second-screen "HIDE MAIN HUD" toggle (see
  // super_metroid-android's docs/retroarch-fork-notes.md and
  // sfc/cpu/dma.cpp's own long comment on g_smwide_hud_hidden). vram is
  // private to this class (see below) with no existing public accessor,
  // so CPU::Channel::transfer (dma.cpp - not a friend of PPU) can't reach
  // it directly the way PPU's own nested classes (Background/Object,
  // friended below) do. This lets it force a word directly into VRAM the
  // instant the toggle flips on mid-room, to blank a mostly-static
  // element (the minimap border/frame - InitializeHud DMAs it once per
  // room entry, not every frame) that would otherwise sit there stale
  // and visible until something else happens to re-upload it - same
  // problem and fix as SM2_SetHudHidden's own real comment on the
  // original from-scratch SNES core this fork's DMA intercept is ported
  // from. Defined out-of-line, after this class closes: vram's own
  // VRAM::operator[] has a deduced (auto&) return type, which C++
  // requires to be fully defined before any use - and this class's own
  // private VRAM struct/vram member (below) is declared well after this
  // point in the class body.
  auto forceBlankVramWord(uint16 address, uint16 data) -> void;

private:
  //ppu.cpp
  alwaysinline auto step() -> void;
  alwaysinline auto step(uint clocks) -> void;

  //io.cpp
  alwaysinline auto addressVRAM() const -> uint16;
  alwaysinline auto readVRAM() -> uint16;
  alwaysinline auto writeVRAM(bool byte, uint8 data) -> void;
  alwaysinline auto readOAM(uint10 address) -> uint8;
  alwaysinline auto writeOAM(uint10 address, uint8 data) -> void;
  alwaysinline auto readCGRAM(bool byte, uint8 address) -> uint8;
  alwaysinline auto writeCGRAM(uint8 address, uint15 data) -> void;
  auto readIO(uint address, uint8 data) -> uint8;
  auto writeIO(uint address, uint8 data) -> void;
  auto updateVideoMode() -> void;

  struct VRAM {
    auto& operator[](uint address) { return data[address & mask]; }
    uint16 data[64 * 1024];
    uint16 mask = 0x7fff;
  } vram;

  uint32 output[512 * 480];
  uint32 lightTable[16][32768];

  struct {
    bool interlace;
    bool overscan;
    uint vdisp;
  } display;

  auto refresh() -> void;

  struct {
    uint4 version;
    uint8 mdr;
  } ppu1, ppu2;

  struct Latch {
    uint16 vram;
     uint8 oam;
     uint8 cgram;
     uint8 bgofsPPU1;
     uint3 bgofsPPU2;
     uint8 mode7;
     uint1 counters;
     uint1 hcounter;
     uint1 vcounter;

    uint10 oamAddress;
     uint8 cgramAddress;
  } latch;

  struct IO {
    //$2100  INIDISP
     uint1 displayDisable;
     uint4 displayBrightness;

    //$2102  OAMADDL
    //$2103  OAMADDH
    uint10 oamBaseAddress;
    uint10 oamAddress;
     uint1 oamPriority;

    //$2105  BGMODE
     uint1 bgPriority;
     uint8 bgMode;

    //$210d  BG1HOFS
    uint16 hoffsetMode7;

    //$210e  BG1VOFS
    uint16 voffsetMode7;

    //$2115  VMAIN
     uint1 vramIncrementMode;
     uint2 vramMapping;
     uint8 vramIncrementSize;

    //$2116  VMADDL
    //$2117  VMADDH
    uint16 vramAddress;

    //$211a  M7SEL
     uint2 repeatMode7;
     uint1 vflipMode7;
     uint1 hflipMode7;

    //$211b  M7A
    uint16 m7a;

    //$211c  M7B
    uint16 m7b;

    //$211d  M7C
    uint16 m7c;

    //$211e  M7D
    uint16 m7d;

    //$211f  M7X
    uint16 m7x;

    //$2120  M7Y
    uint16 m7y;

    //$2121  CGADD
     uint8 cgramAddress;
     uint1 cgramAddressLatch;

    //$2133  SETINI
     uint1 extbg;
     uint1 pseudoHires;
     uint1 overscan;
     uint1 interlace;

    //$213c  OPHCT
    uint16 hcounter;

    //$213d  OPVCT
    uint16 vcounter;
  } io;

  #include "mosaic.hpp"
  #include "background.hpp"
  #include "object.hpp"
  #include "window.hpp"
  #include "screen.hpp"

  Mosaic mosaic;
  Background bg1;
  Background bg2;
  Background bg3;
  Background bg4;
  Object obj;
  Window window;
  Screen screen;

  friend class PPU::Background;
  friend class PPU::Object;
  friend class PPU::Window;
  friend class PPU::Screen;
  friend class System;
  friend class PPUfast;
};

// Out-of-line: see forceBlankVramWord's own declaration/comment above for
// why (VRAM::operator[]'s deduced return type has to be visible first).
alwaysinline auto PPU::forceBlankVramWord(uint16 address, uint16 data) -> void { vram[address] = data; }

extern PPU ppu;
