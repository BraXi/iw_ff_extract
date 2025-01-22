# ffdump_extract
Extract rawfiles from any CoD game after dumping FastFile with `offzip` utility.

It can extract:
* All: `gsc, csc, gsx, atr, txt, script, arena, def, vision, shock, cfg, vehicles`
* Partial 'csv' 
* Totaly empty: 'menu'
To bo honest, this code is very primitive and naive and was written years ago when I was curious about alpha versions of CoD4, but *it just works*. Really, don't judge me. Compile it with whatever you want.

This could likely work with other games such as MW2, BO, MW3 and others, but I have not tested. I was able to extract all rawfiles from all games that I've tested including: 
* CoD2 - Xbox 360
* CoD4:MW - all Alpha versions for Xbox360 (v253,270,290 and later)
* CoD4:MW - PC
* CoDWaW - PC
* CoDWaW - all alpha versions for Xbox360



# USAGE
1. First, you need to deflate `.FF` file with `offzip` utility which will find and extract all zlib blocks it can find in fastfile. 'offzip.exe -a zone.ff x:/zone'
2. Use `extractff.exe` to find and extract all rawfiles from that dump file: `extractff.exe x:/zone_dump.dat_ x:/zone`
3. Enjoy.
