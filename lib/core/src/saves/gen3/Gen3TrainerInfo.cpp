#include "Gen3TrainerInfo.h"

#include "Gen3TextDecoder.h"
#include "utils/ByteUtils.h"

namespace {

const Gen3BadgeLocation &badge_location_for(Gen3GameVersion version) {
  switch (version) {
    case Gen3GameVersion::Emerald:
      return GEN3_BADGE_LOCATION_EMERALD;
    case Gen3GameVersion::FireRedLeafGreen:
      return GEN3_BADGE_LOCATION_FRLG;
    case Gen3GameVersion::RubySapphire:
    case Gen3GameVersion::Unknown:
    default:
      return GEN3_BADGE_LOCATION_RS;
  }
}

uint8_t extract_badges(const uint8_t *large_block, Gen3GameVersion version) {
  Gen3BadgeLocation loc = badge_location_for(version);
  uint8_t badges = 0;

  for (int i = 0; i < 8; i++) {
    int flag_number = loc.badge_flag_start + i;
    int byte_offset = loc.event_flag_base + (flag_number >> 3);
    int bit = flag_number & 7;
    if ((large_block[byte_offset] >> bit) & 1) {
      badges |= (uint8_t)(1 << i);
    }
  }

  return badges;
}

}  // namespace

Trainer gen3_extract_trainer_info(const Gen3ParsedSave &save) {
  Trainer trainer{};

  gen3_text_decode(save.small_block + GEN3_SMALL_OT_NAME_OFFSET, GEN3_SMALL_OT_NAME_LENGTH,
                    trainer.name, sizeof(trainer.name));

  trainer.tid = read_u16_le(save.small_block + GEN3_SMALL_TID_OFFSET);
  trainer.sid = read_u16_le(save.small_block + GEN3_SMALL_SID_OFFSET);
  trainer.played_hours = read_u16_le(save.small_block + GEN3_SMALL_PLAYED_HOURS_OFFSET);
  trainer.played_minutes = save.small_block[GEN3_SMALL_PLAYED_MINUTES_OFFSET];
  trainer.played_seconds = save.small_block[GEN3_SMALL_PLAYED_SECONDS_OFFSET];
  trainer.badges = extract_badges(save.large_block, save.version);

  return trainer;
}
