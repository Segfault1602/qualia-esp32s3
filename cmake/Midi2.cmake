include_guard(GLOBAL)
include(FetchContent)

# The MIDI 2.0 class needs the upstream Stream-message callback.
FetchContent_Declare(tinyusb_midi2
    URL https://codeload.github.com/hathach/tinyusb/tar.gz/3ab3f9209a27eac15c464e31caec355c7b06b5cb
    URL_HASH SHA256=5d90325179946982f0b2177c3a961be0d2e07486dd48e3d5956ea87be3f3831b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR qualia-unused)
FetchContent_MakeAvailable(tinyusb_midi2)

file(READ "${tinyusb_midi2_SOURCE_DIR}/src/class/midi/midi2_device.c" midi2_source)
macro(qualia_replace_midi2 original replacement)
    string(FIND "${midi2_source}" "${original}" match)
    if(match LESS 0)
        message(FATAL_ERROR "Pinned MIDI 2.0 class no longer matches the SET_INTERFACE fix.")
    endif()
    string(LENGTH "${original}" match_length)
    math(EXPR match_end "${match} + ${match_length}")
    string(SUBSTRING "${midi2_source}" ${match_end} -1 remainder)
    string(FIND "${remainder}" "${original}" duplicate)
    if(NOT duplicate LESS 0)
        message(FATAL_ERROR "MIDI 2.0 SET_INTERFACE fix has an ambiguous source match.")
    endif()
    string(REPLACE "${original}" "${replacement}" midi2_source "${midi2_source}")
endmacro()

qualia_replace_midi2("  bool    negotiated;" [=[  bool    negotiated;
  tusb_desc_endpoint_t ep_descriptors[2];]=])
qualia_replace_midi2("      const uint8_t ep_addr = desc_ep->bEndpointAddress;" [=[      const uint8_t ep_addr = desc_ep->bEndpointAddress;
      p_midi->ep_descriptors[tu_edpt_dir(ep_addr)] = *desc_ep;]=])
qualia_replace_midi2("      p_midi->alt_setting = alt;" [=[      p_midi->alt_setting = alt;
      for (uint8_t dir = 0; dir < 2; dir++) {
        usbd_edpt_close(rhport, p_midi->ep_descriptors[dir].bEndpointAddress);
      }]=])
qualia_replace_midi2("      // Re-arm RX endpoint for receiving data after alt setting change" [=[      for (uint8_t dir = 0; dir < 2; dir++) {
        TU_ASSERT(usbd_edpt_open(rhport, &p_midi->ep_descriptors[dir]), false);
      }

      // Re-arm RX endpoint for receiving data after alt setting change]=])

set(QUALIA_MIDI2_CLASS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/qualia_midi2/midi2_device.c")
file(CONFIGURE OUTPUT "${QUALIA_MIDI2_CLASS_SOURCE}" CONTENT "${midi2_source}" @ONLY)
