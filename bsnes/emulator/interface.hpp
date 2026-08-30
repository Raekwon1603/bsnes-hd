#pragma once

namespace Emulator {

struct Interface {
  struct Information {
    string manufacturer;
    string name;
    string extension;
    bool resettable = false;
  };

  struct Display {
    struct Type { enum : uint {
      CRT,
      LCD,
    };};
    uint id = 0;
    string name;
    uint type = 0;
    uint colors = 0;
    uint width = 0;
    uint height = 0;
    uint internalWidth = 0;
    uint internalHeight = 0;
    double aspectCorrection = 0;
  };

  struct Port {
    uint id;
    string name;
  };

  struct Device {
    uint id;
    string name;
  };

  struct Input {
    struct Type { enum : uint {
      Hat,
      Button,
      Trigger,
      Control,
      Axis,
      Rumble,
    };};

    uint type;
    string name;
  };

  //information
  virtual auto information() -> Information { return {}; }

  virtual auto display() -> Display { return {}; }
  virtual auto color(uint32 color) -> uint64 { return 0; }

  //game interface
  virtual auto loaded() -> bool { return false; }
  virtual auto hashes() -> vector<string> { return {}; }
  virtual auto manifests() -> vector<string> { return {}; }
  virtual auto titles() -> vector<string> { return {}; }
  virtual auto title() -> string { return {}; }
  virtual auto load() -> bool { return false; }
  virtual auto save() -> void {}
  virtual auto unload() -> void {}

  //system interface
  virtual auto ports() -> vector<Port> { return {}; }
  virtual auto devices(uint port) -> vector<Device> { return {}; }
  virtual auto inputs(uint device) -> vector<Input> { return {}; }
  virtual auto connected(uint port) -> uint { return 0; }
  virtual auto connect(uint port, uint device) -> void {}
  virtual auto power() -> void {}
  virtual auto reset() -> void {}
  virtual auto run() -> void {}

  //time functions
  virtual auto rtc() -> bool { return false; }
  virtual auto synchronize(uint64 timestamp = 0) -> void {}

  //state functions
  virtual auto serialize(bool synchronize = true) -> serializer { return {}; }
  virtual auto unserialize(serializer&) -> bool { return false; }

  //cheat functions
  virtual auto read(uint24 address) -> uint8 { return 0; }
  //  writes a single byte of system work RAM ($7E:0000-$7F:FFFF) - not the
  //  general bus write() a real cheat-code engine would need (arbitrary
  //  cartridge/SRAM regions map very differently per game/chip, real
  //  complexity a frontend has no business doing on its own - see
  //  target-libretro/libretro.cpp's retro_get_memory_data comment on this
  //  same tradeoff for the read side). Added alongside read() above for the
  //  Super Metroid Android fork's second-screen companion display to write
  //  a live game-state field directly (e.g. which ammo type is armed) the
  //  same way pressing a real controller button would - see that project's
  //  docs/retroarch-fork-notes.md.
  virtual auto writeWorkRam(uint17 address, uint8 data) -> void {}
  virtual auto cheats(const vector<string>& = {}) -> void {}

  //configuration
  virtual auto configuration() -> string { return {}; }
  virtual auto configuration(string name) -> string { return {}; }
  virtual auto configure(string configuration = "") -> bool { return false; }
  virtual auto configure(string name, string value) -> bool { return false; }

  //settings
  virtual auto cap(const string& name) -> bool { return false; }
  virtual auto get(const string& name) -> any { return {}; }
  virtual auto set(const string& name, const any& value) -> bool { return false; }

  virtual auto frameSkip() -> uint { return 0; }
  virtual auto setFrameSkip(uint frameSkip) -> void {}

  virtual auto runAhead() -> bool { return false; }
  virtual auto setRunAhead(bool runAhead) -> void {}
};

}
