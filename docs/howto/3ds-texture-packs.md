# Custom textures for 3DS games

The 3DS runtime can load PNG texture replacements in Citra's naming layout.
This is separate from the texture upscaling filter: a pack supplies replacement
pictures, while the filter enlarges the game's existing textures.

1. Quit the game.
2. Find its runtime data folder: the folder containing that game's `settings.toml`
   and `user` folder. Create a folder called `hd-textures` there.
3. Extract your chosen pack into `hd-textures`. Subfolders can stay. Keep PNG names
   such as `tex1_128x64_0123456789ABCDEF_12.png` unchanged.
4. Start the game again. Open its menu, choose **3DS**, then turn **HD textures** on.

Use a pack made for the game you are playing. Keep X/Y, ORAS and USUM packs in
their respective game folders. ZIP/7z archives must be extracted first. DDS/KTX
files or LayeredFS game-file mods are not converted by the PNG loader.

**No pack** means no supported filenames were found at startup. Check the folder
and naming, then restart the game. The log reports `HD textures: N in the pack at ...`.
If a pack is found but a particular picture stays unchanged, check its game and
hash compatibility. Renaming a hash cannot convert a different hash scheme.

For advanced setups, `ZAKURO_HD_TEXTURES` overrides the runtime's pack folder. A
standalone Linux runtime can be started like this:

```sh
ZAKURO_HD_TEXTURES="/path/to/extracted/pack" pocket-native "/path/to/your-game.3ds"
```

FullDex does not include the game artwork or the packs linked in issue #266.
Those individual packs still need in-game compatibility checks with this runtime;
testing in another emulator alone does not confirm that its texture hashes match.
