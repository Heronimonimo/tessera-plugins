#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::new_music {

// One release, in fixed fields: the list is a tessera::List, one block in PSRAM, never many small strings in the memory
// inside the chip that a picture's download and Wi-Fi need (docs/FIRMWARE_API.md, "Memory").
struct Release {
  enum Kind : uint8_t { ALBUM, EP, SINGLE };
  char title[52] = {};    // the app cuts a text at 48 bytes
  char artist[68] = {};   // the label of the artist's pick, at most 64
  char cover[36] = {};    // Deezer's md5_image, 32 characters
  uint32_t date = 0;
  Kind kind = ALBUM;
};

// The plugin: the tile "releases" and the card "release" a tap on the release in the middle opens. The tile hands the
// card what it shows through here, as the card is made later and knows only the entity and the tile it came from.
class NewMusic : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  Release chosen;              // the release the card shows
  tessera::Text player_name;   // the player's name in Home Assistant, for the card's key
};

}  // namespace esphome::new_music
