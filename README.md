## Textures Unlimited Recolour Maker
A program for automating the creation of config files that use Textures Unlimited to recolour parts in Kerbal Space Program.
Takes a part.cfg file and constructs a pair of apropriate config files, one to convert the part to use a TU texture switch, and one to add recolouring to the part.
_Features classic windows 98 stylings, for a charmingly retro yet practical aesthetic! 😁️_

# Features:
- Automatic part.cfg parsing into a tree of modules.
- Automatic ModelData.txt locating and parsing into a list of materials, as opposed to the list of meshes it starts as.
- Click to copy any of the properties of the part.cfg or ModelData.
- Buttons (![Important Modules Button](https://github.com/ZZetho/pics/blob/main/ModuleTreeRelevantOn.png?raw=true), ![Important materials button](https://github.com/ZZetho/pics/blob/main/ModelsListRelevantOn.png?raw=true)) To show / hide the modules and materials that are relevant to recolouring.
- Automatic generation of materials, texture sets, texture switches and @PART configs to recolour a part.

# To install and run:
- Download and unzip the files.
- Put the Recolour_Maker folder directly inside GameData.
- In the build folder, run either "RecolourMaker.exe" if you are on windows or "RecolourMaker" if you are on linux.

# Use:
- To begin, select a part.cfg file using the file browser.
- Press the ![Recolour Button](https://github.com/ZZetho/pics/blob/main/Recolour.png?raw=true) Recolour button to generate the default and recolour configs.
- Two configs will be created, a default and a recolour one, the default config adds texture switches to the part and creates texture sets to replace the parts materials, the recolour config creates texture sets with materials that can be recoloured and adds them to the texture switches added by the default config.
- Press the ![Copy Button](https://github.com/ZZetho/pics/blob/main/Copy.png?raw=true) Copy button above the default / recolour config to copy that config to your clipboard, which can then be pasted in your .cfg files.

# Roadmap:
- Currently creates a texture switch for each relevant material the part uses, it should detect when those materials are part of variants and as such the texture switcher can be combined.
- Allow selecting a maskTex before generating recolour configs, so that vectors can be automatically calculated.
- Overlays a wireframe of the model onto the parts textures so that it is obvious what materials correspond to where.
- Show a 3D model of the selected part, with labeled meshes and overlaid with maskTex (This'll be real hard).

# Compatiablility:
Currently includes build for x86_64 both Windows and Linux.

# Known bugs:
Fails to correctly parse inline Modules, fotunatley these are extramly rare.

# License:
MIT
