# Sky Corridor / 空の回廊 — playing v0.1.5, NVIDIA build

A free Windows exploration prototype. Walk an empty city above the clouds and use
a small handheld device to collect traces of its former residents. The in-game
interface supports English and Japanese.

This build includes runtimes from the official UE 5.8 DLSS 4.5 plugin **8.7.2**. On supported hardware, select
**DLSS Quality** or another mode in **Graphics & Controls**. Unsupported features
remain unavailable; TSR is also available. Frame generation, ray reconstruction
and Reflex report their support separately. Dynamic frame generation follows the
display refresh rate automatically; it does not guarantee the selected FPS cap.

HDR requires a compatible display and Windows HDR enabled. Match **Display peak
brightness** to your display and adjust **HDR brightness** to control reference
white. For the game's native HDR, keep **Windows HDR On**, **in-game HDR On**, and
**NVIDIA RTX HDR Off for this game**. RTX HDR is a separate conversion path; NVIDIA
requires in-game HDR to be disabled when using it ([NVIDIA App FAQ](https://nvidia.custhelp.com/app/answers/detail/a_id/5521/~/nvidia-app-faq)).
“Softer colors” applies only in SDR; its saved preference returns when HDR is off.
The reported appearance issue is resolved on the reporting user's setup; a possible
RTX HDR interaction is not a confirmed cause. If the result is unsuitable on your
display, turn HDR off to return to SDR. This is not a guarantee for all displays.

## Start and save

Extract the **entire ZIP** into a writable folder. Launch **PLAY.cmd**, kept beside
the **Windows** folder. Do not run it inside the archive. Unreal Editor is not
required. This is a 64-bit Windows / DirectX 12 build; minimum hardware requirements
have not been established.

If the first launch prompts you to install the Microsoft Visual C++ runtime,
review the prompt and complete the installation. If a missing-runtime error prevents
startup without showing that prompt, run the bundled
**Windows\Engine\Extras\Redist\en-us\vc_redist.x64.exe**, follow its installation
screens, then launch **PLAY.cmd** again.

Progress is saved in **PlayData beside PLAY.cmd**. Close the game and back up that
whole folder before updating. Extract an update into a new folder, copy your
backed-up PlayData, and keep the previous version until your progress loads.
Allow additional disk space for saves and screenshots.

## Your first memory

1. Start at the water beside Clock Plaza (**時計広場**).
2. Press **Q**. Select the top-left eye icon, **Observe**.
3. Select **Begin observing**, then close the device with **Q / Esc**.
4. Approach the faint pale trace by the water. Watch briefly and press **E** when prompted.
5. Open **Q → Records** (the book icon) to read it. **Map** (top-right) helps you choose another location.

## Controls

| Key | Action |
|---|---|
| WASD / mouse | Walk / look |
| Shift | Run |
| Space, then Space in the air | Double jump |
| E | Use a nearby object or record a trace when prompted |
| Q | Raise or put away the handheld device |
| Left click / wheel | Tap / scroll on the device |
| Tab | Exploration journal |
| Esc | Back / menu |
| F9 | Photo mode |
| F8 | Screenshot |
| F11 | Fullscreen toggle |

Use the device's **Observe**, **Map**, **Records** and **Camera** apps to explore.
**Friends** is unavailable in normal play. **Video** is disabled and marked **Unavailable in public build**.
The short bottom bar returns to its home screen.

## Explore further

Clock Plaza station connects to the White Tower water city (**白塔の水都**).
Station elevators reach the upper line serving the Cloud Hotel (**雲上ホテル**),
Sky Theatre (**天空シアター**) and airship port (**空中港**). Libraries, a museum,
cafés and rooftops can be entered. A full day takes 30 real minutes.

v0.1.4 removes the elevators' full-height guide masts and the airship port's four
decorative support columns. The floating cabins still serve the same stops, with
their doors, landing floors and guards retained.

**In-game YouTube playback has been discontinued in the public release.** The plaza,
cinema and sky screens remain scenery and do not offer online-video playback or controls.
The cinema and Sky Theatre can still be visited. Older promotional footage may show
the historical playback prototype; it is not a feature of this public-game build.

## Prototype scope

Solo exploration is the main experience. Online rooms are experimental and disabled
by default. External-network multiplayer and physical spatial audio still have
unverified conditions. Built-in menus, handheld apps, memories, destination names, transit guidance and fishing descriptions support English. Online experimental tools are outside the translation scope.

For problem reports, include **v0.1.5 NVIDIA**, Windows/GPU details, the location, action,
and time of day. Remove personal information from logs; do not publish your entire
save folder or account data.

You may record, stream and monetize your gameplay under the included
**GAMEPLAY-VIDEO-PERMISSION.md**, including original music heard during play.
External videos and music retain their owners' rights. Keep
**THIRD-PARTY-NOTICES.md** and **Licenses** with the distribution.
The included **END-USER-TERMS.md** contains the product's Unreal technology terms
and Epic disclaimer.

## Unavailable features in v0.1.5

City browsing/hosting, world eggs/add-ons and the phone Friends app
are marked **In development** (English) or **開発中** (Japanese) and cannot be selected in normal play.
Shared cinema is marked **Unavailable in public build** / **公開版では利用不可** and remains disabled even with experimental flags.
The T shortcut is also blocked. YouTube playback is discontinued, rather than a promised
upcoming feature. Fishing, journals, photos and hotel visits remain available.

## Language

Use **Language / 言語: English → 日本語** on the title screen or in **Graphics & Controls** to switch immediately. In Japanese, choose **Language / 言語: 日本語 → English**. The choice persists in `PlayData/language.txt`, separately from world and discovery data. First launch selects Japanese for a Japanese Windows language, otherwise English. For testing, `-EWLanguage=en` or `-EWLanguage=ja` overrides startup without changing the saved preference.
