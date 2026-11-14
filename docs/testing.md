# Testing

There is no conformance harness yet (it is on the [ROADMAP](../ROADMAP.md)).
What exists are two checks anyone can run, both headless: the dry run, and a
scripted run from the profile screen into an open park. Neither opens a window
on your screens: `--headless` runs on a private desktop that is never shown,
and mutes the sound.

## The dry run

```
build\simthemepark.exe
```

Maps `tp.exe`'s image, binds its imports and stops. Expected:
`[bind] 0x00400000: ... 0 unresolved`, then
`(dry run: mapped and bound; --run enters 0x0067EE10)`. Any `unresolved` is a
missing shim or a lift that did not cover a call target.

## Into a park

```
build\simthemepark.exe --run --headless --watchdog 560 --click 460,36@100 --click 307,136@112 ^
    --key 0x41@120 --key 0x42@122 --click 440,176@130 --click 330,250@175 --shot 540 park.bmp
```

Create New Player, a name typed (`A`, `B`), Instant Action, the Lost Kingdom
island. It passes when `park.bmp` shows the park open, with visitors and the
calendar running ([the screenshot](screenshots/park-lost-kingdom.png) is this
run). The times are seconds from the start: the generated C at `/Od` draws
about 7 frames a second, so the front end takes a while; on a slow machine,
push them later.

The game reads its mouse and keyboard only through buffered DirectInput
(`GetDeviceData`), so the clicks and keys go through pcrecomp's synthetic
DirectInput; a toolkit without it starts the game
but nothing answers.

`--watchdog` ends the run and says where the game was, so a hang shows up as
an address rather than a run that never ends. `--record out.mp4` keeps every
frame (ffmpeg on `PATH`), and `--native-trace` logs each call into Windows.
`tools/br.sh NAME [args]` is the dev loop: build, then one muted headless run
logged to `work\NAME.log`.
