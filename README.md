# Textures Unlimited Recolour Maker
Automates the creation of the config files that [Textures Unlimited](https://github.com/KSPModStewards/TexturesUnlimited) uses to recolour parts in Kerbal Space Program.  
This program takes a part.cfg file and constructs a pair of appropriate config files, one to convert the part to replace the stock material with a TU texture set, and one to add a recolourable texture set to the part.  
_Features classic windows 98 stylings, for a charmingly retro yet practical aesthetic!_ 😁️  
Uses [Raylib](https://github.com/raysan5/raylib) for rendering and [Clay](https://github.com/nicbarker/clay) for building the UI.

## Features:
- Automatic part.cfg parsing into a tree of modules.
- Automatic ModelData.txt locating and parsing into a list of materials, as opposed to the list of meshes it starts as.
- Click to copy any of the properties of the part.cfg or ModelData.
- Buttons (![Important Modules Button](https://github.com/ZZetho/pics/blob/main/ModuleTreeRelevantOn.png?raw=true), ![Important materials button](https://github.com/ZZetho/pics/blob/main/ModelsListRelevantOn.png?raw=true)) To show / hide the modules and materials that are relevant to recolouring.
- Automatic generation of a texture switch and the appropriate texture sets to convert the part to being recolourable for each material, irrelevant materials, such as those for rendering particles or flags are automatically ignored.

## To install and run:
- Make sure Textures Unlimited is installed.
- Download and unzip the files.
- Put the Recolour_Maker folder directly inside GameData.
- Go to GameData/000_TexturesUnlimited/GeneralConfiguration.cfg and change exportUVs to true, then launch KSP (you only need to do this once), this will create the ModelData.txt files that are needed for recolouring, KSP does not need to remain running.
- In the build folder, run either "RecolourMaker.exe" if you are on windows or "RecolourMaker" if you are on linux.

## Use:
- To begin, select a part.cfg file using the file browser on the left, you can click the (![Exit button](https://github.com/ZZetho/pics/blob/main/FileExit.png?raw=true)) to exit a folder.
- Press the recolour button (![Recolour Button](https://github.com/ZZetho/pics/blob/main/Recolour.png?raw=true)) to generate the default and recolour configs.
- Two configs will be created, a default and a recolour one, the default config adds texture switches to the part and creates texture sets to replace the parts materials, the recolour config creates texture sets with materials that can be recoloured and adds them to the texture switches added by the default config.
- Press the copy button (![Copy Button](https://github.com/ZZetho/pics/blob/main/Copy.png?raw=true)) above the default / recolour config to copy that config to your clipboard, which can then be pasted in your .cfg files.

## Roadmap:
- Currently creates a texture switch for each relevant material the part uses, it should detect when those materials are part of variants and, as such the texture switches can be combined.
- Allow selecting a maskTex before generating recolour configs, so that vectors can be automatically calculated.
- Overlays a labeled wireframe of the model onto the parts textures so that it is obvious what materials / meshes correspond to where.
- Show a 3D model of the selected part, with labeled meshes and overlaid with maskTex (This'll be real hard).

## Building from source:
In Recolour_Maker, to build for Windows (x86 64) use the command:  
`cmake -S src -B build --fresh -DCMAKE_TOOLCHAIN_FILE=Windows_64.cmake`  
For Linux (x86 64), use:  
`cmake -S src -B build --fresh -DCMAKE_TOOLCHAIN_FILE=Linux_64.cmake`  
Then, go into build and use the command:  
`make`

## Compatiablility:
Currently only includes build for x86_64 both Windows and Linux.

## Known bugs:
Fails to correctly parse inline Modules, fotunatley these are extremly rare.
Sometimes the materials that are ignored are actually important.

## Credits:
- shadowmage45, for creating [Textures Unlimited](https://github.com/KSPModStewards/TexturesUnlimited).
- Ramon Santamaria, for creating [Raylib](https://github.com/raysan5/raylib).
- Nic Barker, for creating [Clay](https://github.com/nicbarker/clay).

## License:
MIT
