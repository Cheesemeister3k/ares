//Yamaha YM7101
#if defined(PROFILE_PERFORMANCE)
#include "../vdp-performance/vdp.hpp"
#else
struct VDP : Thread {
  Node::Object node;
  Node::Video::Screen screen;
  Node::Setting::Boolean overscan;

  struct Debugger {
    VDP& self;
    Debugger(VDP& self) : self(self) {}

    //debugger.cpp
    auto load(Node::Object) -> void;
    auto unload() -> void;
    auto interrupt(string_view) -> void;
    auto dma(string_view) -> void;
    auto io(n5 register, n8 data) -> void;

    struct Memory {
      Node::Debugger::Memory vram;
      Node::Debugger::Memory vsram;
      Node::Debugger::Memory cram;
    } memory;

    struct Tracer {
      Node::Debugger::Tracer::Notification interrupt;
      Node::Debugger::Tracer::Notification dma;
      Node::Debugger::Tracer::Notification io;
    } tracer;
  } debugger{*this};

  inline auto hcounter() const -> u32 { return state.hcounter; }
  inline auto vcounter() const -> u32 { return state.vcounter; }
  inline auto field() const -> bool { return state.field; }
  inline auto hblank() const -> bool { return state.hblank; }
  inline auto vblank() const -> bool { return state.vblank; }
  inline auto hclock() const -> u32 { return state.hclock; }

  inline auto h32() const -> bool { return io.displayWidth == 0; }  //256-width
  inline auto h40() const -> bool { return io.displayWidth == 1; }  //320-width

  inline auto dclk()  const -> bool { return io.clockSelect == 0; }  //internal clock
  inline auto edclk() const -> bool { return io.clockSelect == 1; }  //external clock

  //vdp.cpp
  auto load(Node::Object) -> void;
  auto unload() -> void;
  auto pixels() -> u32*;
  auto scanline() -> void;
  auto power(bool reset) -> void;

  //main.cpp
  auto step(u32 clocks) -> void;
  auto tick() -> void;
  auto main() -> void;
  auto render() -> void;
  auto hblank(bool) -> void;
  auto vblank(bool) -> void;
  auto mainH32() -> void;
  auto mainH40() -> void;
  auto mainBlankH32() -> void;
  auto mainBlankH40() -> void;
  auto generateCycleTimings() -> void;

  //io.cpp
  auto read(n1 upper, n1 lower, n24 address, n16 data) -> n16;
  auto write(n1 upper, n1 lower, n24 address, n16 data) -> void;

  auto readDataPort() -> n16;
  auto writeDataPort(n16 data) -> void;

  auto readControlPort() -> n16;
  auto writeControlPort(n16 data) -> void;

  struct PSG : SN76489, Thread {
    Node::Object node;
    Node::Audio::Stream stream;

    //psg.cpp
    auto load(Node::Object) -> void;
    auto unload() -> void;

    auto main() -> void;
    auto step(u32 clocks) -> void;

    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    struct IO {
      n1 debugVolumeOverride;
      n2 debugVolumeChannel;
    } io;

  private:
    double volume[16];
  } psg;

  struct IRQ {
    //irq.cpp
    auto poll() -> void;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    struct External {
      n1 enable;
      n1 pending;
    } external;

    struct Hblank {
      n1 enable;
      n1 pending;
      n8 counter;
      n8 frequency;
    } hblank;

    struct Vblank {
      n1 enable;
      n1 pending;
    } vblank;
  } irq;

  struct Cache {
    auto empty() const -> bool { return !upper && !lower; }
    auto full() const -> bool { return upper && lower; }

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n16 data;   //read data
    n1  upper;  //1 = data.byte(1) valid
    n1  lower;  //1 = data.byte(0) valid
  };

  struct Slot {
    auto empty() const -> bool { return !upper && !lower; }
    auto full() const -> bool { return upper && lower; }

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n4  target;   //read/write flag + VRAM/VSRAM/CRAM select
    n16 address;  //word address
    n16 data;     //write data
    n1  upper;    //1 = data.byte(1) valid
    n1  lower;    //1 = data.byte(0) valid
  };

  struct FIFO {
    auto empty() const -> bool { return slots[0].empty(); }
    auto full() const -> bool { return !slots[3].empty(); }

    //fifo.cpp
    auto advance() -> void;

    auto slot() -> void;
    auto read(n4 target, n17 address) -> void;
    auto write(n4 target, n17 address, n16 data) -> void;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    Cache cache;
    Slot  slots[4];
  } fifo;

  struct DMA {
    //dma.cpp
    auto synchronize() -> void;
    auto run() -> void;
    auto load() -> void;
    auto fill() -> void;
    auto copy() -> void;
    auto step() -> void;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n1  active;
    n2  mode;
    n22 source;
    n16 length;
    n8  data;
    n1  wait;
    n1  enable;
  } dma;

  struct Pixel {
    auto above() const -> bool { return priority == 1 && color; }
    auto below() const -> bool { return priority == 0 && color; }

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n6 color;
    n1 priority;
    n1 backdrop;
  };

  struct Layers {
    //layers.cpp
    auto begin() -> void;
    auto hscrollFetch() -> void;
    auto vscrollFetch() -> void;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n2  hscrollMode;
    n15 hscrollAddress;
    n1  vscrollMode;
    n2  nametableWidth;
    n2  nametableHeight;

    i8  vscrollIndex;
  } layers;

  struct Attributes {
    //serialization.cpp
    auto serialize(serializer&) -> void;

    n15 address;
    n16 width;
    n16 height;
    n10 hscroll;
    n10 vscroll;
  };

  struct Window {
    //window.cpp
    auto begin() -> void;
    auto attributesFetch() -> void;
    auto test() const -> bool;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n10 hoffset;
    n1  hdirection;
    n10 voffset;
    n1  vdirection;
    n16 nametableAddress;

    i8  attributesIndex;
  } window;

  struct Layer {
    //layer.cpp
    auto begin() -> void;
    auto attributesFetch() -> void;
    auto mappingFetch() -> void;
    auto patternFetch() -> void;

    auto pixel() -> Pixel;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n10 hscroll;
    n10 vscroll;
    n16 generatorAddress;
    n16 nametableAddress;
    Attributes attributes;
    Pixel pixels[352];
    u128 colors;
    u128 extras;
    n1   windowed[2];

    struct Mapping {
      //serialization.cpp
      auto serialize(serializer&) -> void;

      n15 address;
      n1  hflip;
      n2  palette;
      n1  priority;
    };
    Mapping mappings[2];
    i8  mappingIndex;
    n8  patternIndex;
    n9  pixelCount;
    n9  pixelIndex;
  } layerA, layerB;

  struct Sprite {
    VDP& vdp;

    //the per-scanline sprite limits are different between H40 and H32 modes
    auto objectLimit() const -> u32 { return vdp.latch.displayWidth ? 20 : 16; }
    auto tileLimit()   const -> u32 { return vdp.latch.displayWidth ? 40 : 32; }

    //sprite.cpp
    auto write(n9 address, n16 data) -> void;
    auto begin() -> void;
    auto end() -> void;
    auto mappingFetch() -> void;
    auto patternFetch() -> void;
    auto pixel() -> Pixel;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n16 generatorAddress;
    n16 nametableAddress;
    n1  collision;
    n1  overflow;
    Pixel pixels[320];

    struct Cache {
      //serialization.cpp
      auto serialize(serializer&) -> void;

      n10 y;
      n7  link;
      n2  height;
      n2  width;
    };
    Cache cache[80];

    struct Mapping {
      //serialization.cpp
      auto serialize(serializer&) -> void;

      n1  valid;
      n2  width;
      n2  height;
      n15 address;
      n1  hflip;
      n2  palette;
      n1  priority;
      n9  x;
    };
    Mapping mappings[20];

    n8  mappingCount;

    n9  patternX;
    n8  patternIndex;
    n8  patternSlice;
    n8  patternCount;
    n1  patternStop;

    n7  visible[20];
    n8  visibleLink;
    n8  visibleCount;
    n1  visibleStop;

    n9  pixelIndex;

    n10 vcounter;
  };
  Sprite sprite{*this};

  struct DAC {
    //dac.cpp
    auto begin() -> void;
    auto pixel() -> void;
    auto output(n32 color) -> void;
    auto power(bool reset) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    u32* pixels = nullptr;
  } dac;

  //color.cpp
  auto color(n32) -> n64;

  //serialization.cpp
  auto serialize(serializer&) -> void;

private:
  auto pixelWidth() const -> u32 { return latch.displayWidth ? 4 : 5; }
  auto screenWidth() const -> u32 { return latch.displayWidth ? 320 : 256; }
  auto screenHeight() const -> u32 { return latch.overscan ? 240 : 224; }
  auto frameHeight() const -> u32 { return Region::PAL() ? 312 : 262; }

  //video RAM
  struct VRAM {
    //memory.cpp
    auto read(n16 address) const -> n16;
    auto write(n16 address, n16 data) -> void;

    auto readByte(n17 address) const -> n8;
    auto writeByte(n17 address, n8 data) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    //Mega Drive: 65536x4-bit (x2) =  64KB VRAM
    //Tera Drive: 65536x4-bit (x4) = 128KB VRAM
    n16 memory[65536];  //stored in 16-bit words
    n32 size = 32768;
    n1  mode;  //0 = 64KB, 1 = 128KB
  } vram;

  //vertical scroll RAM
  struct VSRAM {
    //memory.cpp
    auto read(n6 address) const -> n10;
    auto write(n6 address, n10 data) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n10 memory[40];
  } vsram;

  //color RAM
  struct CRAM {
    //memory.cpp
    auto color(n6 address) const -> n9;
    auto read(n6 address) const -> n16;
    auto write(n6 address, n16 data) -> void;

    //serialization.cpp
    auto serialize(serializer&) -> void;

    n9 memory[64];
  } cram;

  //FIFO / DMA command
  struct Command {
    n1  latch;      //write half toggle
    n4  target;     //CD0-CD3
    n1  ready;      //CD4
    n1  pending;    //CD5
    n17 address;    //A0-A16
    n8  increment;  //data increment amount
  } command;

  struct IO {
    //$00  mode register 1
    n1 displayOverlayEnable;
    n1 counterLatch;
    n1 videoMode4;
    n1 leftColumnBlank;

    //$01  mode register 2
    n1 videoMode5;
    n1 overscan;   //0 = 224 lines; 1 = 240 lines
    n1 displayEnable;

    //$07  background color
    n6 backgroundColor;

    //$0c  mode register 4
    n1 displayWidth;  //0 = H32; 1 = H40
    n2 interlaceMode;
    n1 shadowHighlightEnable;
    n1 externalColorEnable;
    n1 hsync;
    n1 vsync;
    n1 clockSelect;  //0 = DCLK; 1 = EDCLK

    //debug registers
    n4 debugAddress;
    n1 debugDisableLayers;
    n2 debugForceLayer;
    n1 debugDisableSpritePhase1;
    n1 debugDisableSpritePhase2;
    n1 debugDisableSpritePhase3;
  } io;

  struct Latch {
    //per-frame
    n1 interlace;
    n1 overscan;

    //per-scanline
    n1 displayWidth;
    n1 clockSelect;
  } latch;

  struct State {
    n16 hcounter;
    n16 vcounter;
    n1  field;
    n1  hblank;
    n1  vblank;
    n16 hclock;
  } state;

//unserialized:
  u8 cyclesH32[2][342], halvesH32[2][171], extrasH32[2][171];
  u8 cyclesH40[2][420], halvesH40[2][210], extrasH40[2][210];
};

extern VDP vdp;
#endif
