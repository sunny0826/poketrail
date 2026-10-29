[中文](usage.md) | **English**

# Usage guide

## Exploration and capture

Once storage is available, set the date, time, and UTC offset, then choose Bulbasaur, Charmander, or Squirtle as your partner. You can postpone clock sync. Total steps and exploration continue, but steps and capture times recorded while the date is unknown are not filled in later. The starter gift and free browsing of the Pokédex do not create first capture cards.

Forest, Coast, and Cave have different encounter pools. Forest covers Grass, Bug, Poison, Fairy, Normal, and Fighting; Coast covers Water, Ice, Flying, and Electric; Cave covers Fire, Rock, Ground, Steel, Dark, Ghost, Psychic, and Dragon. Pokémon with two types belong to the routes for both types. Every 200–500 steps, with a new random target for each encounter, the device saves an encounter and gives a short vibration. Ordinary exploration only offers base forms in the explicit common pool: 73 Kanto species and 51 Johto species. Legendary Pokémon, Mythical Pokémon, the Dratini family, and Larvitar are reserved for future separate events. They currently have no acquisition entry point.

After a successful capture, that species permanently leaves the encounter pools of all routes. Evolution and restarting do not restore its eligibility. You can encounter a species again after a defeat, retreat, failed capture, or abandonment. The starter gift does not count as a capture, so you can still capture that species once. When you have captured all species on a route, you can switch routes. Extra exploration steps are retained.

You can leave an existing encounter for later and continue accumulating steps. Switching routes only affects the next encounter. A defeat or retreat does not cost you your partner. Poké Balls are unlimited; the menu and throwing page show only a ball icon.

Each side has three health segments in a rhythm battle, with alternating attack and dodge actions. The normal window is ±180 ms, and the perfect window is ±70 ms. Each beat accepts only the first action. The wrong button, pressing both buttons together, or a timeout counts as a miss. A battle lasts at most six rounds. Open capture practice from the menu to face a random Pokémon and go through the full battle, two throws, and capture animation. Timing judgments and probabilities match real encounters, but practice consumes no Poké Balls. Success does not add anything to the Pokédex, capture records, or achievements.

After a victory, each encounter allows at most two Poké Balls, and a successful capture ends the encounter immediately. For common Pokémon, the normal / nice / perfect capture rates are 85% / 92% / 98%. For rare Pokémon, they are 70% / 80% / 90%. Missing the window gives a 0% chance. Even a perfect throw can fail.

After a victory, you can keep the result and return to the home page to walk at any time. Later, return through the menu to continue capturing in the same encounter. You can also confirm that you want to abandon it.

## Partners and evolution

You can own at most one Pokémon of each species and carry one partner at a time. Only the carried partner earns training steps. "My partners" lists the Pokémon you currently own in Pokédex number order. You can change your carried partner and view training balances and evolution requirements.

Level-based evolutions cost the original level difference × 500 steps, counting from level 1 for a base form. Friendship, day/night, held-item, trade, and branch evolutions all cost 10000 steps. Once the threshold is reached, you must confirm evolution manually.

| Evolution chain | Cost at each stage |
| --- | --- |
| Bulbasaur → Ivysaur → Venusaur | 7500, then 8000 |
| Charmander → Charmeleon → Charizard | 7500, then 10000 |
| Squirtle → Wartortle → Blastoise | 7500, then 10000 |
| Magikarp → Gyarados | 9500 |
| Pikachu → Raichu | 10000 |
| Chikorita → Bayleef → Meganium | 7500, then 8000 |
| Cyndaquil → Quilava → Typhlosion | 6500, then 11000 |
| Totodile → Croconaw → Feraligatr | 8500, then 6000 |
| Togepi → Togetic | 10000 |
| Tyrogue → Hitmontop / Hitmonlee / Hitmonchan | 10000 for the selected branch |
| Eevee → Vaporeon / Jolteon / Flareon / Espeon / Umbreon | 10000 for the selected branch |

Evolution replaces the original form, while the Pokédex keeps its "Previously obtained" record. Remaining training steps stay in the source species' training account; the target uses its own account. A source species that has already been captured cannot be captured again. A starter gift that has not yet been captured can still be captured once and use its original account balance. Eevee has five branches (Vaporeon, Jolteon, Flareon, Espeon, and Umbreon), and Tyrogue has three (Hitmontop, Hitmonlee, and Hitmonchan). Each save allows only one branch for each source species. Choose the branch in partner details; the choice does not depend on RTC day/night or Attack and Defense values.

If you already own the target, the entire operation is rejected. Both Pokémon and all step balances stay unchanged. Evolution does not deduct total steps, exploration steps, or balls, and does not add capture counts or first capture cards. Changing partners and evolving are limited to ordinary exploration or an encounter waiting for battle. If a save retry or clock sync confirmation is pending, or a battle or throw is in progress, finish the current flow first.

After confirmation and a successful save, an evolution animation plays for about 5 seconds. The original form gathers light, the two silhouettes alternate, and the new form appears. The new partner's details then open. After releasing the confirmation buttons, press either side button or tap to skip to the new form. Act again to open its details. Skipping does not undo evolution. If saving fails, an error appears first; the animation plays only after a successful retry of the same evolution. Restarting during the animation keeps the saved new partner, without replaying the animation or deducting training steps again.

"Preview evolution" at the bottom of a Pokédex entry plays that species' evolution animation. For Eevee, you can select Vaporeon, Jolteon, Flareon, Espeon, or Umbreon; for Tyrogue, you can select any of its three branches. You can preview without owning the species or accumulating training steps. Preview is available even with a new save or a storage error. Preview does not change partners, training balances, or capture records. When it finishes or you skip it, you return to the original Pokédex entry. Species with no further evolution do not show this option.

## Page controls

The home page shows the carried partner, route scene, and today's steps. If the date is trusted but the day's record is incomplete, it shows "Recorded today". If the date is unknown or abnormal, it shows "Today --". This does not mean total steps have been lost. From the main menu, open "Resources and data", then "Date and time". Set the correct time and select "Save and sync clock". After a successful sync, today's statistics resume. Only totals are retained for the period when the date was unknown. Steps, routes, Poké Balls, training, and encounter information are also grouped on the "Resources and data" page.

Press either side button or tap the home page to open the menu. That action does not also navigate or confirm. Release all buttons and lift your finger before the next action. Select "Return to exploration" on the third menu page, or tap the top of the screen, to return home.

In ordinary menus, releasing the left button moves up, and releasing the right button moves down. Holding both buttons with any overlap confirms; you can press them one after the other. After confirmation, release both buttons before continuing. Visible touch buttons activate when you lift your finger. Pressing and then swiping horizontally does not activate them. Menus with multiple pages support left/right swipes and the previous/next page buttons at the bottom. You can also swipe horizontally between record details. Tap the top to go back one level.

| Page | Controls |
| --- | --- |
| Battle / capture practice | Left button to dodge, right button to attack; touching the left/right half of the screen does the same |
| Pause | Tap the top or hold both buttons for 800 ms to pause after the current beat ends |
| Throw menu | Left button to cycle through prepare / keep walking / abandon, right button to confirm; touch is also supported |
| Throw timeline | After preparing, hold the right button or touch and hold for a charged throw; release to throw; a timeout also counts as one throw |
| Practice throw | Same as the throw timeline; two throws per encounter, no Poké Balls consumed, and success does not add a Pokédex entry |
| Partner details | Choose a carried partner or evolution target; evolution requires another confirmation |
| Evolution animation | Press either side button or tap to skip to the new form; act again to open its details |
| Records | Page left/right; tap the bottom to return, or select the return option and confirm |
| Clock sync | Select a field and confirm to edit; left decreases, right increases, both buttons finish editing; save at the end |
| Pokédex | Left button / swipe right for the previous Pokémon, right button / swipe left for the next; tap the top to return |
| Pokédex interaction | Tap the partner in the center for a small hop, a heart, and a short vibration; the animation continues |
| Pokédex evolution preview | Tap "Preview evolution" at the bottom; press either side button or tap to skip to the new form; act again to return to the Pokédex |
| Short press of the power key | Triggers on press: while awake, enters power saving (except during the active flows below); while asleep, wakes the screen only, without triggering a page action |

The Pokédex wraps through entries 1–251. Holding a button does not advance repeatedly, and pressing both buttons in the same frame does not change entries. Short drags, vertical swipes, and releasing a long press do not count as taps.

After 30 seconds of inactivity, the device automatically enters power saving. You can also briefly press the power key or select "Power saving now" on the third menu page. While the screen is off, the device continues counting steps, reading the clock, and saving. Encounters still trigger a vibration. When no vibration is active, saving is idle, and no USB host is connected, the main controller briefly enters light sleep until the next sample. A side button or touch wakes the controller early. With a USB host connected, the screen can still turn off, but the controller does not enter light sleep, to keep the serial connection active. Wake the screen with a side button, touch, or a short press of the power key. The wake action only lights the screen; release all buttons and lift your finger before continuing. Steps are not counted while the device is fully shut down. Double-pressing the power key triggers hardware shutdown through the M5PM1; see the [hardware notes](../README.en.md#hardware).

Active battles, practice, throw timelines, and evolution animations (including Pokédex previews) keep the screen on. A short press of the power key does not turn it off during these flows. The screen can turn off while paused, waiting to throw, or editing clock sync fields. The state and draft values are retained when you wake it.

Holding the power key for about 2 seconds and releasing it after the internal green LED lights still enters hardware download mode. This is an inherent device behavior; see the [M5Stack download mode instructions](https://docs.m5stack.com/en/arduino/stopwatch/program).

The battery level is estimated from voltage and may read high while charging. It turns red at 20% or below, and shows "--" when unavailable.

## Dates and saves

Midnight only switches the daily statistics. Total steps, remaining exploration steps, partners, and permanent achievements are retained. The capture log stores first capture cards for 251 species, the 32 most recent captures, and capture counts for each species. Daily summaries retain the 14 most recent dates with records. Daily milestones are 1000 / 3000 / 5000 steps; lifetime milestones are 10000 / 50000 / 100000 steps.

The RTC stores UTC, and historical records retain the offset used at the time. Recalibrating within the same day does not clear progress. Moving the date backward or detecting an abnormal time puts the date into an unknown state. To correct a mistakenly set future date, select "Correct date" during clock sync. Clock sync first saves a transaction awaiting confirmation, then writes to and reads back the RTC, and finally saves the confirmation. If a restart finds an unfinished transaction, you must confirm again.

The LittleFS dual-slot save normally saves ordinary step progress about every 30 seconds during safe phases. If step events have continued within the last 2 seconds, saving is deferred until the step events stop or 120 seconds have passed since the last periodic save. Encounters, victories, throws, and achievements are committed separately. If saving fails, the same result is frozen for retry. Retrying does not reroll the result, and rewards are not shown before the commit succeeds. Sudden power loss can lose unsaved steps. Active timelines and storage faults can still delay saving, so 120 seconds is not a data-loss limit for all situations.

If mounting fails, the device shows "Storage not initialized". It does not automatically format storage or overwrite old saves, and you can still browse the Pokédex. Corrupt saves, newer versions, and I/O errors each have separate messages, and the files are preserved. If one save slot is corrupt, the other valid slot can be recovered.

Upgrading from schema1 preserves partners, capture history, total steps, and the exploration balance. Training starts at 0. Unfinished encounters with evolved forms or event species are closed, refunding the 500 exploration steps charged under the old rules. Encounters with common base forms are retained. Unresolved common encounters with species already captured are closed without extra rewards or refunds. After upgrading to the current schema5, new encounters have unlimited Poké Balls. In-flight encounters that retain the old throwing rules continue checking and deducting historical inventory. They earn one ball for every 1000 total steps walked after the migration baseline. The catalog expansion from schema2 to schema3 preserves existing Kanto fields and initializes only #152–251 as not obtained, with 0 training steps. The subsequent upgrade to schema4 rebuilds the random encounter accounts, generates the first 200–500-step target, and upgrades the current encounter's ruleVersion from 2 to 3. Schema5 assigns routes by type. If an in-flight encounter's old route no longer belongs to that species, it changes to the first matching route. Existing capture records and the player's selected route stay unchanged. For details, see the [Save migration contract (Chinese only)](design/save-migration.md).

## On-device test mode

Test mode helps developers and maintainers reproduce problems and is outside ordinary gameplay. For the separate test save, hidden entry point, console controls, storage, restarting, and exiting, see [On-device test mode (Chinese only)](usage-test-mode.md). Test data is not written back to the regular save slots. The normal save before entering test mode can still update the regular save. Loading the regular save when exiting does not commit a startup record.

## Terminology

| Chinese term | English term used here |
| --- | --- |
| 遭遇 | encounter |
| 捕捉 | capture |
| 培养步 | training steps |
| 精灵球 | Poké Ball |
| 携带者 | carried partner |
| 伙伴 | partner |
| 图鉴 | Pokédex |
| 存档槽位 | save slot |
| 路线 | route |
| 遭遇池 | encounter pool |
| 节奏对战 | rhythm battle |
| 蓄力投球 | charged throw |
| 校时 | clock sync |
| 首次捕捉 | first capture |
| 里程碑 | milestone |
| 培养账户 | training account |
| 规则版本 | ruleVersion |
| 存档版本 | schema version |
| 省电 | power saving / screen-off |
| 双槽存档 | dual-slot save |
