# KTX2 fixture

`stylized_albedo.ktx2` is an ETC1S+sRGB+mipmap conversion of Axmol's existing
24x24 Windows test logo. It was generated with Basis Universal v2.00; the v2.1
runtime deliberately retains backward transcode compatibility.

`stylized_albedo_base.ktx2` uses the same source and settings without `-mipmap`.
It covers glTF samplers that request mip filtering for a base-level-only image.

Generation command:

```sh
basisu -file Square44x44Logo.targetsize-24_altform-unplated.png \
  -output_file stylized_albedo.ktx2 -ktx2 -mipmap -q 64

basisu -file Square44x44Logo.targetsize-24_altform-unplated.png \
  -output_file stylized_albedo_base.ktx2 -ktx2 -q 64
```
