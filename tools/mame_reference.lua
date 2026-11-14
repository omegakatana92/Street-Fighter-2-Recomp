-- MAME 0.264 reference capture for the Street Fighter II port (sf2, sf2ce, sf2hf).
--
--   mkdir -p h nv
--   SF2_FRAMES=3600 SF2_EVERY=10 SF2_INPUTS=inputs.txt \
--   mame sf2 -nodrc -video none -sound none -nothrottle \
--        -nvram_directory nv -autoboot_script tools/mame_reference.lua
--
-- Writes h/rNNNNN.bin every SF2_EVERY frames (every frame up to SF2_ALL_UNTIL):
-- main work RAM 0xFF0000-0xFFFFFF and graphics RAM 0x900000-0x92FFFF as
-- big-endian words, then the Z80's 2 KB RAM at 0xD000.  Snapshots every
-- SF2_SNAP frames.  SF2_INPUTS is a file of "FRAME MASK" lines with MASK in
-- the bit order of sf2::IN_* (runtime/include/sf2/machine.h); MAME applies
-- set_value() at the next frame, so compare with sf2_headless --mame-latency.
local frame = 0
local N = tonumber(os.getenv("SF2_FRAMES") or "3600")
local every = tonumber(os.getenv("SF2_EVERY") or "10")
local all_until = tonumber(os.getenv("SF2_ALL_UNTIL") or "0")
local snap = tonumber(os.getenv("SF2_SNAP") or "300")
local names = {"P1 Up","P1 Down","P1 Left","P1 Right","P1 Jab Punch","P1 Strong Punch","P1 Fierce Punch",
  "P1 Short Kick","P1 Forward Kick","P1 Roundhouse Kick","1 Player Start",
  "P2 Up","P2 Down","P2 Left","P2 Right","P2 Jab Punch","P2 Strong Punch","P2 Fierce Punch",
  "P2 Short Kick","P2 Forward Kick","P2 Roundhouse Kick","2 Players Start",
  "Coin 1","Coin 2","Service 1","Service Mode"}
local fields = {}
for tag, port in pairs(manager.machine.ioport.ports) do
  for name, f in pairs(port.fields) do fields[name] = f end
end
for i, n in ipairs(names) do
  if not fields[n] then print("mame_reference.lua: no input field '" .. n .. "'") end
end
local ev = {}
local input_file = os.getenv("SF2_INPUTS")
if input_file then
  for line in io.lines(input_file) do local a, b = line:match("(%d+) (%d+)"); ev[#ev+1] = {tonumber(a), tonumber(b)} end
end
local ei = 1
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local zp = manager.machine.devices[":audiocpu"].spaces["program"]
fs = emu.add_machine_frame_notifier(function()
  frame = frame + 1
  while ei <= #ev and ev[ei][1] == frame do
    local m = ev[ei][2]
    for i, n in ipairs(names) do
      local f = fields[n]
      if f then f:set_value(((m >> (i - 1)) & 1)) end
    end
    ei = ei + 1
  end
  if frame <= all_until or frame % every == 0 then
    local t = {}
    for a = 0, 0xFFFE, 2 do t[#t+1] = string.pack(">I2", sp:read_u16(0xFF0000 + a)) end
    for a = 0, 0x2FFFE, 2 do t[#t+1] = string.pack(">I2", sp:read_u16(0x900000 + a)) end
    for a = 0, 0x7FF do t[#t+1] = string.pack("B", zp:read_u8(0xD000 + a)) end
    local f = io.open(string.format("h/r%05d.bin", frame), "wb"); f:write(table.concat(t)); f:close()
  end
  if snap > 0 and frame % snap == 0 then manager.machine.video:snapshot() end
  if frame >= N then manager.machine:exit() end
end)
