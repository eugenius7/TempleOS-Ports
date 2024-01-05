# TODO

* can we backface cull earlier?
* visflag culling regular models
* animation transition/blending
* levels
* mobs
* weapons
* clip triangles against side planes to try to avoid frag distance == 0 issues

# Bugs

* shader.c:146: get_lightmap_value_interp: Assertion `uv.x <= 1.0' failed.`
* triangle line artifacts in shadow map
* some type of race condition in starting TempleOS
  * looks like a race condition related to realloc/malloc
  * it's fixed when I stop mallocing, or I calloc instead of malloc
  * playing sound seems to resolve the issue, related to audio thread?
* Chromium browsers (Linux only?) have bad performance on first page load, refreshing fixes. Doesn't seem to affect unoptimized builds.
* wireframe of clipped models is weird
* maybe create bsp model based off of a global vischeck or something to more gracefully handle invisible brushes
* `__builtin_ceilf` in combination with LTO causes infinite loop
* door has bad visleafs?
* doors have bad clipnodes?
* luxmap is wrong if texture is world-aligned and not object-aligned
* textbox stays on screen when quit to main menu

# Ideas

* Deferred Shading

# Shader Ideas

* https://www.shadertoy.com/user/rasmuskaae
* https://www.shadertoy.com/view/3lGBzW
* https://www.shadertoy.com/view/4sX3RM
* https://www.shadertoy.com/view/4slXWn
* https://www.shadertoy.com/view/XdSGzR
* https://www.shadertoy.com/view/XdXGRM
* https://www.shadertoy.com/view/Xdc3RX
* https://www.shadertoy.com/view/flSBR3
* https://www.shadertoy.com/view/ldscWH
* https://www.shadertoy.com/view/lsX3WM

# Notes

* cglm modified with const parameters where needed to fix warnings

# Making Grayscale Textures

* Convert to grayscale
* Use GIMP's "Brightness & Contrast" to increase contrast
* Save as 8bit grayscale

# Scaling Normal Maps

* Use Krita's Normalization filter after scaling to ensure values are properly normalized
* Don't forget to save as 8bit RGB with sRGB builtin profile, use exiftool to strip ICC profile and other crap Krita inserts
