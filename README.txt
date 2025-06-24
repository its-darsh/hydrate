HYDRATE
====

Hydrate is a Hyprland plugin that allows users to apply custom GLSL shaders to individual windows, rather than the entire screen.

NOTE: This plugin is built for the slightly outdated Hyprland v0.39.0 since it is the version is personally use and maintain locally.

====
INSTALLATION
====

To build the plugin. Clone this repository, navigate into it and then run:

$ make

The above command will generate a "hydrate.so" file. You’ll need to reference this shared library in your Hyprland configuration like so:

plugin = /path/to/hydrate.so


====
PLUGIN OPTIONS
====

plugin:hydrate:fragment-shader    (string,  defaults to ""): Path to the fragment shader for the plugin to use.
plugin:hydrate:damage-everything  (bool, defaults to false): Should the plugin damage everything on each change? Solves some problems related to damage artifacts.
plugin:hydrate:render-every-frame (bool, defaults to false): Should the plugin rerender (pass windows into the shader) on every frame (not wait till a change in window contents)?

====
UNIFORM DETAILS
====

NOTE: Uniforms can change (e.g. name or structure of the value held) during the development of this plugin, make sure to read changes before updating.

varying vec2      v_texcoord: Normalized texture coordinate for the current fragment, ranging from (0.0, 0.0) at the top-left to (1.0, 1.0) at the bottom-right of the entire monitor. This is your primary tool for mapping textures to the screen.

sampler2D                tex: The rendered content of the window itself (including decorations like borders but excluding its shadow). This is the main texture you will be transforming or blending.
sampler2D      backgroundTex: A snapshot of everything on the screen behind the target window. This includes the wallpaper, other windows, and layers. This is what you sample from to create effects like blur, transparency, or distortion of the background.

vec2             monitorSize: Full dimensions in pixel (width, height) of the monitor the window is currently on (e.g. vec2(1920.0, 1080.0)). Useful for converting normalized values (e.g. v_texcoord) back into absolute pixel coordinates.
vec2              windowSize: Pixel dimensions (width, height) of the target window (including its decorations). This value accounts for the monitor's scale.
vec2               windowPos: Absolute pixel coordinates (x, y) of the top-left corner of the target window, relative to the monitor's top-left corner. This value also accounts for the monitor's scale.

float            borderWidth: Width of the window's border in pixels, adjusted for the monitor's scale. This can be used to create effects that interact with the border.
vec4             borderColor: Window's border color in the form vec4(r, g, b, a) (the values are normalized). This represents the primary color of the gradient if one is used.

float                   time: Timer that counts up since the start of the shader. Not using the render-every-frame option would result in your shader being not refreshed on every frame.

====
PROJECT'S TODO
====

[ ] Fix damage issues and animation tracking.
[ ] Ability to select the version of the shader through a configuration option.
[ ] Background texture(s.)
[ ] More rendering metadata uniforms (e.g. rounding, blur and noise data.)
[ ] Add a configuration option for damaging windows on each change.
[ ] Ability to pass uniforms directly from the Hyprland configuration.
[ ] Recompile the shader on file change.
[ ] Ability to modify Hyprland's internal shaders (e.g. blur passes, decorations, etc.)
[ ] Ability to limit the shader to specific window rules.


====
SUPPORT ME
====

If you like things I do, consider leaving a tip through my ko-fi: https://ko-fi.com/its-darsh
