# Textures Unlimited Recolour Maker
Automates the creation of the config files that [Textures Unlimited](https://github.com/KSPModStewards/TexturesUnlimited) uses to recolour parts in Kerbal Space Program.  
This program takes a part.cfg file and constructs a pair of appropriate config files, one to convert the part from stock materials to a TU texture set, and one to add a recolourable texture set to the part.  
Uses [Raylib](https://github.com/raysan5/raylib) for rendering and [Clay](https://github.com/nicbarker/clay) for building the UI.  
_Features classic windows 98 stylings, for a charmingly retro yet practical aesthetic!_ 😁️  

## Features:
- Automatic part.cfg parsing into a tree of modules.
- Automatic ModelData.txt locating and parsing into a list of materials, as opposed to the list of meshes it starts as.
- Click to copy any of the properties of the part.cfg or ModelData.
- Buttons (![Important Modules Button](https://github.com/ZZetho/pics/blob/main/ModuleTreeRelevantOn.png?raw=true), ![Important materials button](https://github.com/ZZetho/pics/blob/main/ModelsListRelevantOn.png?raw=true)) To show / hide the modules and materials that are relevant to recolouring.
- Automatic generation of a texture switch and the appropriate texture sets to convert the part to being recolourable. Irrelevant materials, such as those for rendering particles or flags are automatically ignored.

## To install and run:
- Make sure Textures Unlimited is installed.
- Download and unzip the files.
- Put the Recolour_maker folder directly inside GameData.
- Go to GameData/000_TexturesUnlimited/GeneralConfiguration.cfg and change exportUVs to true, then launch KSP (you only need to do this once), this will create the ModelData.txt files that are needed for recolouring, KSP does not need to remain running.
- In the build folder, run either "RecolourMaker.exe" if you are on windows or "RecolourMaker" if you are on linux.

## Use:
- To begin, select a part.cfg file using the file browser on the left, you can click the (![Exit button](https://github.com/ZZetho/pics/blob/main/FileExit.png?raw=true)) to exit a folder.
- Press the recolour button (![Recolour Button](https://github.com/ZZetho/pics/blob/main/Recolour.png?raw=true)) to generate the default and recolour configs.
- Two configs will be created, a default and a recolour one, the default config adds a texture switch to the part and creates a texture set to replace the parts materials, the recolour config creates a texture set with materials that can be recoloured and adds it to the texture switch added by the default config. The recolour config will probaly be offscreen to the right, hold Control to scroll horizontally and Shift to scroll faster.
- Press the copy button (![Copy Button](https://github.com/ZZetho/pics/blob/main/Copy.png?raw=true)) above the default / recolour config to copy that config to your clipboard, which can then be pasted in your .cfg files.
- Not all properties of the recolour config are automated yet, you have to set the normalisation vectors and mod requirements yourself. Also, if the part uses one of the KSP/Diffuse shaders, then you will also have to supply a specular map to the recolour texture set, the default one included in any of the TURD mods will work fine.
- Replace \<RECOLOURING_MASK\> with the path to your recolour masks.
- Also note that the texture sets include replacements for all the materials a part uses, so if (for example) the materials are supposed to be used for variants the texture sets will try and apply them all at the same time, this can be fixed by splitting the materials up into seperate texture sets.

## Roadmap:
- Currently combines all materials into a single texture set, how materials are used (ie: variants) should be detected and a number of texture sets should be made to reflect this.
- Allow selecting a maskTex before generating recolour configs, so that vectors can be automatically calculated.
- Mod requirements can be selected, allowing fully automated config generation.
- Overlays a labeled wireframe of the model onto the parts textures so that it is obvious what materials / meshes correspond to where.
- Show a 3D model of the selected part, with labeled meshes and overlaid with maskTex (This'll be real hard).

## Building from source:
In Recolour_Maker, use the command:  
`cmake -S src -B build --fresh` (this will take a few minutes)  
Then, go into build and use the command:  
`make`

## Compatiablility:
Currently only includes builds for x86_64 both Windows and Linux, but being written in pure C, builds for other platforms will be easy, let me know if you want them.

## Known bugs:
Fails to correctly parse inline Modules, fotunatley these are extremly rare.  
Sometimes the materials that are ignored are actually important.

## Reporting bugs:
Report bugs on this repositories issues page ([Here](https://github.com/ZZetho/Recolour_maker/issues)), or the KSP forum thread ([Here]()), Please include RecolourMaker.log (in the build folder), the part you were trying to recolour and what mod it is from in your report, thx 😘️.

## Credits:
- shadowmage45, for creating [Textures Unlimited](https://github.com/KSPModStewards/TexturesUnlimited).
- Ramon Santamaria, for creating [Raylib](https://github.com/raysan5/raylib).
- Nic Barker, for creating [Clay](https://github.com/nicbarker/clay).

## License:
MIT, excluding Clay, which keeps its original licence.
